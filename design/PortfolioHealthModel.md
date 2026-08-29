# Portfolio-Health Scoring Model

The health score is intentionally a **heuristic**, not a risk model. It
compresses three orthogonal signals into a single 0–100 number so a
trader can eyeball the state of their book in one glance.

---

## 1. Inputs

For each `Position` with `quantity != 0`:

| Symbol | Definition                                                      |
| ------ | --------------------------------------------------------------- |
| `mv_i` | `lastPrice_i * quantity_i * multiplier_i`   (signed)             |
| `w_i`  | `\|mv_i\| / Σ_j \|mv_j\|`                                       |
| `inv_i`| `\|averagePrice_i * quantity_i * multiplier_i\|`                |
| `u_i`  | `unrealised_i` (broker-reported MTM)                             |

Portfolio-level aggregates:

| Metric              | Formula                                            |
| ------------------- | -------------------------------------------------- |
| Gross exposure      | $E_g = \sum_i |mv_i|$                              |
| Net exposure        | $E_n = \sum_i mv_i$                                |
| Invested capital    | $C = \sum_i inv_i$                                 |
| Total P&L           | $P = \sum_i u_i + \sum_i realised_i$               |
| P&L %               | $\rho = P / C \cdot 100$                           |
| Herfindahl index    | $H = \sum_i w_i^2  \in [1/N, 1]$                   |
| Top-1 concentration | $\kappa = \max_i |mv_i| / E_g \cdot 100$           |
| Win rate            | $r = \\#\\{i : u_i > 0\\} / N$                     |

---

## 2. Score

$$
\text{score} = \underbrace{\big(25 + \text{clip}(\rho, -25, +25)\big)}_{\text{P\\&L (0–50)}}
             + \underbrace{30 r}_{\text{win rate (0–30)}}
             + \underbrace{20 (1 - H)}_{\text{diversification (0–20)}}
$$

The result is clipped to $[0, 100]$ and rounded.

### 2.1 Weights & rationale

| Component        | Weight | Why                                                   |
| ---------------- | ------ | ----------------------------------------------------- |
| P&L %            | 50     | Direct outcome — a healthy portfolio makes money.     |
| Win rate         | 30     | Guards against one-lucky-name masking a bad book.     |
| Diversification  | 20     | Penalises single-name / single-sector concentration.  |

### 2.2 P&L clip band

$\rho$ is clipped to $\pm 25\%$ so a runaway winner or loser cannot
saturate the score all by itself. The band is symmetric on purpose:
a +30% and a +25% book should register roughly the same health.

### 2.3 Grade table

| Score  | Grade |
| ------ | ----- |
| ≥ 85   | A     |
| 70–84  | B     |
| 55–69  | C     |
| 40–54  | D     |
| < 40   | F     |

---

## 3. Alert rules

Alerts are surfaced independently of the score so a *green* book can
still flag a red risk (e.g. one huge winner driving concentration).

| Trigger                                             | Message                                            |
| --------------------------------------------------- | -------------------------------------------------- |
| No open positions                                   | "No open positions."                               |
| $\kappa > 40\\%$                                    | "High single-name concentration (<κ>%)."           |
| $H > 0.35$                                          | "Portfolio is poorly diversified (HHI=<H>%)."      |
| $\rho < -10\\%$                                     | "Drawdown exceeds 10% of invested capital."        |
| losers > 75 % of open positions                     | "Majority of open positions are underwater."       |

All thresholds live at the top of
[`src/PortfolioHealth.cpp`](../src/PortfolioHealth.cpp) — change them in
one place if the model needs to be re-tuned.

---

## 4. Limitations

* Uses **broker-reported `unrealised`** — options premium accounting
  follows Upstox conventions. Verify vs your own MTM before acting.
* Assumes INR base currency; multi-currency books aren't supported yet.
* No sector / instrument-class weighting: two different names in the
  same sector still count as diversification.
* Correlations are ignored. The Herfindahl index treats names as
  independent — a book of six Nifty banks looks diversified here but
  behaves like one position under stress.
* Historical volatility, beta, VAR are **not** modelled. If you need
  those, wire a market-data provider into a second pass and add a fourth
  component to the score.

## 5. News Sentiment Signals

The web UI also provides a separate news signal; it is not included in the
0–100 health-score formula. News is fetched for the Upstox holdings account
and filtered to symbols listed in `config/holding.csv`.

For each article, the C++ web server scans the heading and summary for
positive and negative keywords and computes:

$$s = \frac{positive\ matches - negative\ matches}{positive\ matches + negative\ matches}$$

The score is `0` when no keywords match. A score of `0.30` or higher is
`positive`; `-0.30` or lower is `negative`; all other scores are `neutral`.
The resulting signals are deliberately advisory: `Positive signal - review`,
`Risk alert - review`, or `Hold / monitor`. The application has no order
placement API and does not automatically buy or sell securities.
