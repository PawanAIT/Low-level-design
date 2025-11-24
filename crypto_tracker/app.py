from flask import Flask, render_template
from flask_socketio import SocketIO
import asyncio
from collections import defaultdict, deque
import json
import threading
import websockets
from datetime import datetime, timedelta, timezone
import logging
import os

# Set up logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')

app = Flask(__name__, template_folder='templates', static_folder='static')
socketio = SocketIO(app, async_mode='threading')

def format_trade(trade):
    price = float(trade['px'])
    size = float(trade['sz'])
    value = price * size
    time = datetime.fromtimestamp(trade['time'] / 1000, tz=timezone.utc)
    liquidation = "Y" if trade.get('liquidation', False) else "N"
    side = "Buy" if trade['side'] == 'B' else "Sell"
    return {
        "users": trade.get('users', []),
        "coin": trade['coin'],
        "time": time.isoformat(),
        "side": side,
        "price": f"{price:.4f}",
        "size": f"{size:.4f}",
        "value": f"{value:,.2f}",
        "liquidation": liquidation
    }

async def handle_message(message, coins):
    """Processes incoming messages and emits them to clients via websocket."""
    try:
        data = json.loads(message)
        if 'channel' in data and data['channel'] == 'trades':
            trades = data['data']
            for trade in trades:
                coin = trade['coin']
                if coin in coins:
                    # Emit trade data to clients
                    socketio.emit('new_trade', format_trade(trade))
    except json.JSONDecodeError:
        logging.error("Failed to decode JSON from message.")
    except Exception as e:
        logging.exception(f"An error occurred in handle_message: {e}")

async def websocket_client(uri, coins):
    """The main websocket connection and message handling loop."""
    while True:
        try:
            async with websockets.connect(uri, ping_interval=20, close_timeout=10) as websocket:
                for coin in coins:
                    await websocket.send(json.dumps({
                        "method": "subscribe",
                        "subscription": {"type": "trades", "coin": coin}
                    }))
                
                async for message in websocket:
                    await handle_message(message, coins)
        except (websockets.ConnectionClosed, websockets.WebSocketException) as e:
            logging.error(f"WebSocket connection issue: {e}. Retrying in 5 seconds...")
            await asyncio.sleep(5)
        except Exception as e:
            logging.exception("An unexpected error occurred in websocket_client")
            await asyncio.sleep(5)

@app.context_processor
def inject_strftime():
    return dict(strftime=lambda d, fmt: d.strftime(fmt))

@app.route('/')
def index():
    return render_template('index.html')


def run_websocket():
    coins = ["BTC", "ETH"]
    uri = "wss://api-ui.hyperliquid.xyz/ws"
    loop = asyncio.new_event_loop()
    asyncio.set_event_loop(loop)
    loop.run_until_complete(websocket_client(uri, coins))

if __name__ == '__main__':
    websocket_thread = threading.Thread(target=run_websocket, daemon=True)
    websocket_thread.start()
    socketio.run(app, host='localhost', port=5006)
