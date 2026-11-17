import struct
import torch
import torch.nn as nn
import numpy as np

NNUE_INPUTS = 768
NNUE_ACCUMULATOR_SIZE = 256
NNUE_QA = 181          # 181^2 = 32761 fits in int16_t for ARM NEON SCReLU
NNUE_QB = 64
NNUE_OUTPUT_SCALE = 400.0
NNUE_MAGIC = 0x33764E4E  # "NNv3"


class Simple768NNUE(nn.Module):
    """
    Modern shallow SCReLU NNUE architecture: (768 -> 256)x2 -> 1.
    Uses Squared Clipped ReLU (SCReLU = clamp(x, 0, 1)^2) for strong
    non-linear piece interactions with ultra-fast ARM NEON inference.
    """
    def __init__(self):
        super().__init__()
        self.ft = nn.Linear(NNUE_INPUTS, NNUE_ACCUMULATOR_SIZE)
        self.out = nn.Linear(NNUE_ACCUMULATOR_SIZE * 2, 1)

    def forward(self, us_features, them_features):
        acc_us = torch.clamp(self.ft(us_features), 0.0, 1.0)
        acc_them = torch.clamp(self.ft(them_features), 0.0, 1.0)
        x = torch.cat([acc_us * acc_us, acc_them * acc_them], dim=1)
        return self.out(x)

    def clamp_weights(self):
        """Keep weights safely within quantization headroom."""
        with torch.no_grad():
            self.ft.weight.clamp_(-1.98, 1.98)
            self.out.weight.clamp_(-1.98, 1.98)

    def export_quantized(self, filename="nnue.bin"):
        """
        Exports quantized NNv3 binary matching engine_src/nnue.cpp:
          - ft_weights[768][256] : int16 (scale QA = 181)
          - ft_biases[256]       : int16 (scale QA = 181)
          - out_weights[512]     : int16 (scale QB = 64)
          - out_bias             : int32 (scale QA * QA * QB)
        """
        ft_w = self.ft.weight.detach().cpu().numpy()           # [256, 768]
        ft_b = self.ft.bias.detach().cpu().numpy()             # [256]
        out_w = self.out.weight.detach().cpu().numpy().flatten()  # [512]
        out_b = float(self.out.bias.detach().cpu().numpy().flatten()[0])

        ft_w_q = np.clip(np.round(ft_w.T * NNUE_QA), -32767, 32767).astype(np.int16)
        ft_b_q = np.clip(np.round(ft_b * NNUE_QA), -32767, 32767).astype(np.int16)
        out_w_q = np.clip(np.round(out_w * NNUE_QB), -32767, 32767).astype(np.int16)
        out_b_q = int(np.round(out_b * NNUE_QA * NNUE_QA * NNUE_QB))

        with open(filename, "wb") as f:
            f.write(struct.pack("<I", NNUE_MAGIC))
            f.write(ft_w_q.tobytes())
            f.write(ft_b_q.tobytes())
            f.write(out_w_q.tobytes())
            f.write(struct.pack("<i", out_b_q))

        total_bytes = 4 + ft_w_q.nbytes + ft_b_q.nbytes + out_w_q.nbytes + 4
        print(f"Successfully exported quantized NNUE v3 (SCReLU) to {filename} ({total_bytes} bytes)")
