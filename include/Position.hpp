#pragma once

#include <string>

namespace folio {

// Single position record as returned by Upstox
// GET /v2/portfolio/short-term-positions.
// Field names mirror the JSON payload documented at
// https://upstox.com/developer/api-documentation/get-positions
struct Position {
    std::string exchange;         // e.g. "NSE", "BSE", "NFO"
    std::string tradingSymbol;    // e.g. "INFY"
    std::string instrumentToken;  // e.g. "NSE_EQ|INE009A01021"
    std::string product;          // "I" (intraday), "D" (delivery/CNC), "M" (margin)

    long   quantity = 0;          // net quantity (buy - sell)
    long   overnightQuantity = 0; // carry-forward qty from previous session
    long   dayBuyQuantity = 0;
    long   daySellQuantity = 0;

    double averagePrice = 0.0;    // avg price of the net position
    double lastPrice = 0.0;       // current LTP
    double closePrice = 0.0;      // previous-day close (used for day P&L)
    double buyPrice = 0.0;
    double sellPrice = 0.0;
    double buyValue = 0.0;
    double sellValue = 0.0;

    double unrealised = 0.0;      // MTM P&L on open qty
    double realised = 0.0;        // booked P&L for the day
    double pnl = 0.0;             // unrealised + realised (as sent by broker)
    double value = 0.0;           // signed exposure of the open leg

    int multiplier = 1;           // lot / contract multiplier

    // Derived helpers ------------------------------------------------------
    double marketValue() const { return lastPrice * quantity * multiplier; }
    double exposure()    const { return marketValue() >= 0 ? marketValue() : -marketValue(); }
    double totalPnl()    const { return unrealised + realised; }
};

} // namespace folio
