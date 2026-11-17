import struct
import torch
import torch.nn as nn
import numpy as np

NNUE_INPUTS = 768
NNUE_ACCUMULATOR_SIZE = 256
NNUE_HIDDEN1_SIZE = 32
NNUE_OUTPUT_SCALE = 400.0
NNUE_MAGIC = 0x32764E4E  # "NNv2"


class Simple768NNUE(nn.Module):
    def __init__(self):
        super().__init__()
        # Feature transformer: 768 -> 256
        self.ft = nn.Linear(NNUE_INPUTS, NNUE_ACCUMULATOR_SIZE)

        # Hidden layer 1: 512 -> 32
        self.l1 = nn.Linear(NNUE_ACCUMULATOR_SIZE * 2, NNUE_HIDDEN1_SIZE)

        # Hidden layer 2: 32 -> 32
        self.l2 = nn.Linear(NNUE_HIDDEN1_SIZE, NNUE_HIDDEN1_SIZE)

        # Output layer: 32 -> 1 (outputs scaled score z, where cp = z * 400.0)
        self.out = nn.Linear(NNUE_HIDDEN1_SIZE, 1)

    def forward(self, us_features, them_features):
        """
        us_features: [batch, 768] multi-hot binary
        them_features: [batch, 768] multi-hot binary
        Returns scaled score z where centipawns = z * NNUE_OUTPUT_SCALE (400.0).
        """
        acc_us = torch.clamp(self.ft(us_features), 0.0, 1.0)
        acc_them = torch.clamp(self.ft(them_features), 0.0, 1.0)

        x = torch.cat([acc_us, acc_them], dim=1)
        x = torch.clamp(self.l1(x), 0.0, 1.0)
        x = torch.clamp(self.l2(x), 0.0, 1.0)
        return self.out(x)

    def clamp_weights(self):
        """
        Keep int8-quantized layer weights within [-1.98, 1.98] so quantization
        at scale 64.0 (int8 range [-127, 127]) suffers zero clipping distortion.
        """
        with torch.no_grad():
            self.l1.weight.clamp_(-1.98, 1.98)
            self.l2.weight.clamp_(-1.98, 1.98)
            self.out.weight.clamp_(-1.98, 1.98)

    def export_quantized(self, filename="nnue.bin"):
        """
        Exports the PyTorch model to binary format matching engine_src/nnue.cpp.
        Math invariant:
          - ft output: float [0, 1] -> int16/uint8 [0, 127] (scale 127)
          - l1/l2 weights: scale 64 (int8), bias: scale 64 * 127 (int32)
          - out weights: scale 64 (int8), bias: scale 64 * 127 (int32)
          - C++ returns (output * 400) / (64 * 127)
        """
        ft_w = self.ft.weight.detach().cpu().numpy()  # [256, 768]
        ft_b = self.ft.bias.detach().cpu().numpy()    # [256]

        l1_w = self.l1.weight.detach().cpu().numpy()  # [32, 512]
        l1_b = self.l1.bias.detach().cpu().numpy()    # [32]

        l2_w = self.l2.weight.detach().cpu().numpy()  # [32, 32]
        l2_b = self.l2.bias.detach().cpu().numpy()    # [32]

        out_w = self.out.weight.detach().cpu().numpy().flatten()  # [32]
        out_b = float(self.out.bias.detach().cpu().numpy().flatten()[0])

        ft_w_q = np.clip(np.round(ft_w.T * 127.0), -32767, 32767).astype(np.int16)
        ft_b_q = np.clip(np.round(ft_b * 127.0), -32767, 32767).astype(np.int16)

        l1_w_q = np.clip(np.round(l1_w.T * 64.0), -127, 127).astype(np.int8)
        l1_b_q = np.round(l1_b * 64.0 * 127.0).astype(np.int32)

        l2_w_q = np.clip(np.round(l2_w.T * 64.0), -127, 127).astype(np.int8)
        l2_b_q = np.round(l2_b * 64.0 * 127.0).astype(np.int32)

        out_w_q = np.clip(np.round(out_w * 64.0), -127, 127).astype(np.int8)
        out_b_q = int(np.round(out_b * 64.0 * 127.0))

        with open(filename, "wb") as f:
            f.write(struct.pack("<I", NNUE_MAGIC))
            f.write(ft_w_q.tobytes())
            f.write(ft_b_q.tobytes())
            f.write(l1_w_q.tobytes())
            f.write(l1_b_q.tobytes())
            f.write(l2_w_q.tobytes())
            f.write(l2_b_q.tobytes())
            f.write(out_w_q.tobytes())
            f.write(struct.pack("<i", out_b_q))

        total_bytes = (
            struct.calcsize("<I")
            + ft_w_q.nbytes + ft_b_q.nbytes
            + l1_w_q.nbytes + l1_b_q.nbytes
            + l2_w_q.nbytes + l2_b_q.nbytes
            + out_w_q.nbytes + 4
        )
        print(f"Successfully exported quantized NNUE v2 model to {filename} ({total_bytes} bytes)")
