#!/usr/bin/env python3
"""Gomoku AI — Python entry point.

Usage:
    python main.py              # 主程序
    python main.py --online     # 线上对战客户端
"""

import sys, os

if __name__ == "__main__":
    if "--online" in sys.argv or os.environ.get("GOMOKU_ONLINE") == "1":
        from gomoku.client_online import main as online_main
        online_main()
    else:
        from gomoku.app import GomokuApp
        app = GomokuApp()
        app.run()
