import os
import random
import chess
import torch
import torch.nn as nn
from torch.utils.data import DataLoader
from model import Simple768NNUE
from dataset import ChessPositionDataset, extract_features_from_board

def generate_bootstrap_dataset(num_positions=5000):
    """
    Generates a bootstrap dataset of positions and evaluations using self-play random walks
    and classical piece-square heuristics to initialize the network with basic chess principles.
    """
    print(f"Generating {num_positions} bootstrap training positions...")
    positions = []
    
    piece_values = {
        chess.PAWN: 100,
        chess.KNIGHT: 320,
        chess.BISHOP: 330,
        chess.ROOK: 500,
        chess.QUEEN: 900,
        chess.KING: 0
    }
    
    for _ in range(num_positions // 20):
        board = chess.Board()
        for ply in range(30):
            if board.is_game_over():
                break
            moves = list(board.legal_moves)
            if not moves:
                break
            
            # Simple heuristic evaluation from perspective of side to move
            us = board.turn
            us_val = sum(len(board.pieces(pt, us)) * val for pt, val in piece_values.items())
            them_val = sum(len(board.pieces(pt, not us)) * val for pt, val in piece_values.items())
            
            # Central control bonus
            central_sqs = [chess.D4, chess.E4, chess.D5, chess.E5]
            center_bonus = sum(30 for sq in central_sqs if board.piece_at(sq) and board.piece_at(sq).color == us)
            
            score = float((us_val - them_val) + center_bonus)
            positions.append((board.fen(), score))
            
            # Choose move with slight bias toward captures/central moves
            move = random.choice(moves)
            board.push(move)
            
    print(f"Generated {len(positions)} positions.")
    return positions

def train_nnue(epochs=5, batch_size=64, lr=1e-3, export_path="nnue.bin"):
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print(f"Training on device: {device}")
    
    positions = generate_bootstrap_dataset(num_positions=4000)
    dataset = ChessPositionDataset(positions)
    dataloader = DataLoader(dataset, batch_size=batch_size, shuffle=True)
    
    model = Simple768NNUE().to(device)
    optimizer = torch.optim.AdamW(model.parameters(), lr=lr, weight_decay=1e-4)
    criterion = nn.MSELoss()
    
    # Sigmoid scaling constant for centipawn to win probability conversion
    SCALE = 400.0
    
    model.train()
    for epoch in range(epochs):
        total_loss = 0.0
        batches = 0
        
        for us_feat, them_feat, target in dataloader:
            us_feat = us_feat.to(device)
            them_feat = them_feat.to(device)
            target = target.to(device)
            
            optimizer.zero_grad()
            pred = model(us_feat, them_feat)
            
            # Sigmoid loss
            pred_prob = torch.sigmoid(pred / SCALE)
            target_prob = torch.sigmoid(target / SCALE)
            loss = criterion(pred_prob, target_prob)
            
            loss.backward()
            optimizer.step()
            
            total_loss += loss.item()
            batches += 1
            
        avg_loss = total_loss / max(batches, 1)
        print(f"Epoch {epoch + 1}/{epochs} - Loss: {avg_loss:.5f}")
        
    model.export_quantized(export_path)
    return model

if __name__ == "__main__":
    import sys
    export_file = sys.argv[1] if len(sys.argv) > 1 else "nnue.bin"
    train_nnue(epochs=3, batch_size=64, export_path=export_file)
