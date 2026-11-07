import struct
import torch
import torch.nn as nn
import numpy as np

NNUE_INPUTS = 768
NNUE_ACCUMULATOR_SIZE = 256
NNUE_HIDDEN1_SIZE = 32
NNUE_MAGIC = 0x31764E4E # "NNv1"

class Simple768NNUE(nn.Module):
    def __init__(self):
        super().__init__()
        # Feature transformer: 768 -> 256
        self.ft = nn.Linear(NNUE_INPUTS, NNUE_ACCUMULATOR_SIZE)
        
        # Hidden layer 1: 512 -> 32
        self.l1 = nn.Linear(NNUE_ACCUMULATOR_SIZE * 2, NNUE_HIDDEN1_SIZE)
        
        # Hidden layer 2: 32 -> 32
        self.l2 = nn.Linear(NNUE_HIDDEN1_SIZE, NNUE_HIDDEN1_SIZE)
        
        # Output layer: 32 -> 1
        self.out = nn.Linear(NNUE_HIDDEN1_SIZE, 1)

    def forward(self, us_features, them_features):
        """
        us_features: [batch, 768] multi-hot sparse/dense binary
        them_features: [batch, 768] multi-hot sparse/dense binary
        """
        # Feature transformer
        acc_us = torch.clamp(self.ft(us_features), 0.0, 1.0)
        acc_them = torch.clamp(self.ft(them_features), 0.0, 1.0)
        
        # Concatenate perspectives
        x = torch.cat([acc_us, acc_them], dim=1)
        
        # Hidden layers
        x = torch.clamp(self.l1(x), 0.0, 1.0)
        x = torch.clamp(self.l2(x), 0.0, 1.0)
        
        # Centipawn output
        score = self.out(x)
        return score

    def export_quantized(self, filename="nnue.bin"):
        """
        Exports the PyTorch model to binary file format matching engine C++ inference.
        """
        ft_w = self.ft.weight.detach().cpu().numpy() # [256, 768]
        ft_b = self.ft.bias.detach().cpu().numpy()   # [256]
        
        l1_w = self.l1.weight.detach().cpu().numpy() # [32, 512]
        l1_b = self.l1.bias.detach().cpu().numpy()   # [32]
        
        l2_w = self.l2.weight.detach().cpu().numpy() # [32, 32]
        l2_b = self.l2.bias.detach().cpu().numpy()   # [32]
        
        out_w = self.out.weight.detach().cpu().numpy().flatten() # [32]
        out_b = float(self.out.bias.detach().cpu().numpy().flatten()[0])

        # Quantize Feature Transformer: float [0, 1] -> int16 [0, 127]
        # Weight shape for C++: feature_weights[768][256]
        ft_w_q = np.clip(np.round(ft_w.T * 127.0), -32767, 32767).astype(np.int16)
        ft_b_q = np.clip(np.round(ft_b * 127.0), -32767, 32767).astype(np.int16)
        
        # Quantize Layer 1: weights int8 [512][32], bias int32 [32]
        l1_w_q = np.clip(np.round(l1_w.T * 64.0), -127, 127).astype(np.int8)
        l1_b_q = np.round(l1_b * 64.0 * 127.0).astype(np.int32)
        
        # Quantize Layer 2: weights int8 [32][32], bias int32 [32]
        l2_w_q = np.clip(np.round(l2_w.T * 64.0), -127, 127).astype(np.int8)
        l2_b_q = np.round(l2_b * 64.0 * 127.0).astype(np.int32)
        
        # Quantize Output: weights int8 [32], bias int32
        out_w_q = np.clip(np.round(out_w * 16.0), -127, 127).astype(np.int8)
        out_b_q = int(np.round(out_b * 16.0 * 127.0))
        
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
            
        print(f"Successfully exported quantized NNUE model to {filename} ({struct.calcsize('<I') + ft_w_q.nbytes + ft_b_q.nbytes + l1_w_q.nbytes + l1_b_q.nbytes + l2_w_q.nbytes + l2_b_q.nbytes + out_w_q.nbytes + 4} bytes)")
