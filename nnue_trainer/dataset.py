import chess
import torch
from torch.utils.data import Dataset
import numpy as np

def extract_features_from_board(board: chess.Board):
    """
    Extracts Simple768 sparse multi-hot features from the perspective of
    the side to move (us) and the opponent (them).
    """
    us_features = np.zeros(768, dtype=np.float32)
    them_features = np.zeros(768, dtype=np.float32)
    
    us = board.turn
    them = not us
    
    for sq in chess.SQUARES:
        piece = board.piece_at(sq)
        if piece is None:
            continue
            
        p_type = piece.piece_type - 1 # 0..5
        
        # 'us' perspective
        p_idx_us = p_type if piece.color == us else (p_type + 6)
        sq_us = sq if us == chess.WHITE else (sq ^ 56)
        us_features[p_idx_us * 64 + sq_us] = 1.0
        
        # 'them' perspective
        p_idx_them = p_type if piece.color == them else (p_type + 6)
        sq_them = sq if them == chess.WHITE else (sq ^ 56)
        them_features[p_idx_them * 64 + sq_them] = 1.0
        
    return us_features, them_features

class ChessPositionDataset(Dataset):
    def __init__(self, positions):
        """
        positions: list of (fen_string, score_or_result)
        score_or_result: either centipawn score from side to move's perspective,
        or W/D/L outcome in [0.0, 1.0] from side to move's perspective.
        """
        self.positions = positions

    def __len__(self):
        return len(self.positions)

    def __getitem__(self, idx):
        fen, target = self.positions[idx]
        board = chess.Board(fen)
        us_feat, them_feat = extract_features_from_board(board)
        return (
            torch.from_numpy(us_feat),
            torch.from_numpy(them_feat),
            torch.tensor([target], dtype=torch.float32)
        )

