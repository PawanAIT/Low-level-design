document.addEventListener('DOMContentLoaded', function() {
    const socket = io();

    const summaryTableBody = document.querySelector('#summary-table tbody');
    const whalesContainer = document.getElementById('whales-container');
    const resetButton = document.getElementById('reset-button');

    let tradeStats = {
        'BTC': { 'buys': 0, 'sells': 0, 'buy_volume': 0.0, 'sell_volume': 0.0 },
        'ETH': { 'buys': 0, 'sells': 0, 'buy_volume': 0.0, 'sell_volume': 0.0 }
    };
    let whaleTrades = {};

    function formatVolume(volume) {
        return `$${parseFloat(volume).toLocaleString('en-US', { minimumFractionDigits: 2, maximumFractionDigits: 2 })}`;
    }

    function updateSummary() {
        summaryTableBody.innerHTML = ''; // Clear existing summary
        for (const coin in tradeStats) {
            const coinStats = tradeStats[coin];
            const totalTrades = coinStats.buys + coinStats.sells;
            const row = `
                <tr>
                    <td>${coin}</td>
                    <td>${totalTrades}</td>
                    <td>${coinStats.buys}</td>
                    <td>${coinStats.sells}</td>
                    <td>${formatVolume(coinStats.buy_volume)}</td>
                    <td>${formatVolume(coinStats.sell_volume)}</td>
                </tr>
            `;
            summaryTableBody.innerHTML += row;
        }
    }

    function updateWhales() {
        whalesContainer.innerHTML = '<h2>Whale Trades (Over $100,000)</h2>';

        // Calculate total volume for each whale
        let whaleVolumes = [];
        for (const user in whaleTrades) {
            const trades = whaleTrades[user];
            const totalVolume = trades.reduce((sum, trade) => sum + parseFloat(trade.value.replace(/,/g, '')), 0);
            whaleVolumes.push({ user, totalVolume, trades });
        }

        // Sort whales by total volume and get top 5
        whaleVolumes.sort((a, b) => b.totalVolume - a.totalVolume);
        const top5Whales = whaleVolumes.slice(0, 5);

        for (const whale of top5Whales) {
            const panel = document.createElement('div');
            panel.className = 'whale-panel';

            let tradesHtml = `
                <h3>Whale: <span class="whale-address">${whale.user}</span></h3>
                <table>
                    <thead>
                        <tr>
                            <th>Time</th>
                            <th>Coin</th>
                            <th>Side</th>
                            <th>Price</th>
                            <th>Size</th>
                            <th>Value (USD)</th>
                            <th>Liq</th>
                        </tr>
                    </thead>
                    <tbody>
            `;
            // Show most recent 10 trades first
            whale.trades.slice(-10).reverse().forEach(trade => {
                const sideClass = trade.side === 'Buy' ? 'buy' : 'sell';
                tradesHtml += `
                    <tr>
                        <td>${new Date(trade.time).toLocaleString()}</td>
                        <td>${trade.coin}</td>
                        <td class="${sideClass}">${trade.side}</td>
                        <td>${trade.price}</td>
                        <td>${trade.size}</td>
                        <td>$${trade.value}</td>
                        <td>${trade.liquidation}</td>
                    </tr>
                `;
            });
            tradesHtml += `</tbody></table>`;
            panel.innerHTML = tradesHtml;
            whalesContainer.appendChild(panel);
        }
    }

    socket.on('new_trade', function(trade) {
        const coin = trade.coin;
        const value = parseFloat(trade.value.replace(/,/g, ''));

        // Update stats
        if (tradeStats[coin]) {
            if (trade.side === 'Buy') {
                tradeStats[coin].buys += 1;
                tradeStats[coin].buy_volume += value;
            } else {
                tradeStats[coin].sells += 1;
                tradeStats[coin].sell_volume += value;
            }
        }

        // Update whale trades
        if (value > 100000) {
            trade.users.forEach(user => {
                if (!whaleTrades[user]) {
                    whaleTrades[user] = [];
                }
                whaleTrades[user].push(trade);
                if (whaleTrades[user].length > 50) {
                    whaleTrades[user].shift();
                }
            });
        }

        updateSummary();
        updateWhales();
    });

    resetButton.addEventListener('click', () => {
        tradeStats = {
            'BTC': { 'buys': 0, 'sells': 0, 'buy_volume': 0.0, 'sell_volume': 0.0 },
            'ETH': { 'buys': 0, 'sells': 0, 'buy_volume': 0.0, 'sell_volume': 0.0 }
        };
        whaleTrades = {};
        updateSummary();
        updateWhales();
    });

    updateSummary(); // Initial render
});
