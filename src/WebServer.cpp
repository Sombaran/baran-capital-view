#include "WebServer.hpp"
#include "TechnicalIndicators.hpp"

#include <nlohmann/json.hpp>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <stdexcept>
#include <random>
#include <thread>
#include <unordered_set>
#include <vector>

namespace folio {

using nlohmann::json;

#ifndef PORTFOLIO_HEALTH_VERSION
#define PORTFOLIO_HEALTH_VERSION "unknown"
#endif

std::string normalizeLoginCode(const std::string& value) {
    if (value.empty()) return {};
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = value.find_last_not_of(" \t\r\n");
    std::string trimmed = value.substr(first, last - first + 1);
    std::string decoded;
    decoded.reserve(trimmed.size());
    for (std::size_t index = 0; index < trimmed.size(); ++index) {
        if (trimmed[index] == '%' && index + 2 < trimmed.size() &&
            std::isxdigit(static_cast<unsigned char>(trimmed[index + 1])) &&
            std::isxdigit(static_cast<unsigned char>(trimmed[index + 2]))) {
            const auto digit = [](char character) {
                if (character >= '0' && character <= '9') return character - '0';
                if (character >= 'a' && character <= 'f') return character - 'a' + 10;
                return character - 'A' + 10;
            };
            decoded.push_back(static_cast<char>(digit(trimmed[index + 1]) * 16 + digit(trimmed[index + 2])));
            index += 2;
        } else {
            decoded.push_back(trimmed[index] == '+' ? ' ' : trimmed[index]);
        }
    }
    const auto finalFirst = decoded.find_first_not_of(" \t\r\n");
    if (finalFirst == std::string::npos) return {};
    const auto finalLast = decoded.find_last_not_of(" \t\r\n");
    return decoded.substr(finalFirst, finalLast - finalFirst + 1);
}

bool validateLoginCode(const std::string& submittedValue,
                       const std::string& configuredValue) {
    const std::string submitted = normalizeLoginCode(submittedValue);
    const std::string configured = normalizeLoginCode(configuredValue);
    if (submitted.empty() || configured.empty()) return false;
    if (submitted.size() != configured.size()) return false;
    volatile unsigned char diff = 0;
    for (std::size_t i = 0; i < submitted.size(); ++i) {
        diff |= static_cast<unsigned char>(submitted[i]) ^ static_cast<unsigned char>(configured[i]);
    }
    return diff == 0;
}

std::string normalizeSymbol(std::string symbol) {
    const auto first = symbol.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = symbol.find_last_not_of(" \t\r\n");
    symbol = symbol.substr(first, last - first + 1);
    std::transform(symbol.begin(), symbol.end(), symbol.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::toupper(character));
                   });
    return symbol;
}

std::vector<std::string> deeperAnalysisCategoryOrder() {
    return {"Neutral news", "No recent news", "going good", "invest more", "sell it off"};
}

std::string normalizeDecisionAction(const std::string& value) {
    const std::string trimmed = value;
    if (trimmed.rfind("Consider", 0) == 0) return "Consider adding";
    if (trimmed.rfind("Do not", 0) == 0) return "Do not add";
    if (trimmed.rfind("Hold", 0) == 0) return "Hold / wait";
    if (trimmed.rfind("Buy", 0) == 0) return "Buy / review";
    if (trimmed.rfind("Sell", 0) == 0) return "Sell / review";
    return trimmed.empty() ? "Hold / wait" : trimmed;
}

namespace {

const char* loginPage() {
    return R"HTML(<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Portfolio Login</title><style>body{margin:0;min-height:100vh;display:grid;place-items:center;background:linear-gradient(135deg,#e9f1ed,#f7eee5);color:#172126;font:16px Georgia,serif}.login{width:min(360px,calc(100% - 40px));padding:30px;background:#fffdf8;border:1px solid #ded9cf;box-shadow:0 16px 40px #17212620}.kicker{font:700 11px Arial,sans-serif;letter-spacing:2px;text-transform:uppercase;color:#0d7774}h1{font-size:34px;font-weight:500;margin:10px 0 25px}label{display:block;font:700 11px Arial,sans-serif;letter-spacing:1px;text-transform:uppercase;color:#6b777b;margin-bottom:8px}input{width:100%;padding:12px;border:1px solid #bfc8c5;font:16px Arial,sans-serif;box-sizing:border-box}button{width:100%;margin-top:16px;padding:12px;border:0;background:#0d7774;color:white;font-weight:700;cursor:pointer}.error{color:#b74747;font:13px Arial,sans-serif;margin-top:14px}</style></head><body><main class="login"><div class="kicker">Som Baran Gupta / Upstox</div><h1>Portfolio login</h1><form id="form"><label for="code">Secret code</label><input id="code" type="password" autocomplete="one-time-code" required><button>Login</button><div class="error" id="error"></div></form><script>const form=document.querySelector('#form'),code=document.querySelector('#code'),error=document.querySelector('#error');form.onsubmit=async e=>{e.preventDefault();error.textContent='';let r=await fetch('/api/login',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'code='+encodeURIComponent(code.value)});if(r.ok)location.href='/';else error.textContent='Invalid secret code.'}</script></main></body></html>)HTML";
}

const char* dashboardEnhancements() {
    return R"JS(<script>(function(){const nav=document.querySelector('.tabs'),view=document.querySelector('#view'),status=document.querySelector('#status');if(!nav||!view)return;const tab=document.createElement('button');tab.className='tab';tab.dataset.tab='health';tab.setAttribute('role','tab');tab.setAttribute('aria-selected','false');tab.textContent='Data health';nav.insertBefore(tab,nav.lastElementChild);const state=()=>{try{return JSON.parse(localStorage.getItem('baran-capital-view-alert-state')||'{}')}catch(e){return{}}};const save=value=>localStorage.setItem('baran-capital-view-alert-state',JSON.stringify(value));const showHealth=async()=>{nav.querySelectorAll('.tab').forEach(x=>{const active=x===tab;x.classList.toggle('active',active);x.setAttribute('aria-selected',String(active))});view.innerHTML='<div class="panel loading">Checking data health...</div>';try{const started=Date.now(),[holdings,news]=await Promise.all([fetch('/api/holdings').then(x=>x.json()),fetch('/api/news').then(x=>x.json())]),rows=holdings.data||[],articles=Object.values(news.data||{}).reduce((n,x)=>n+x.length,0),alerts=(news.alerts||[]).length,missing=rows.filter(x=>!x.last_price).length;status.textContent='Data health checked in '+(Date.now()-started)+' ms';view.innerHTML='<div class="panel"><h2>Data health</h2><p>Read-only checks for the latest portfolio and news snapshot.</p><div class="grid"><div class="metric"><span class="label">Holdings loaded</span><b>'+rows.length+'</b></div><div class="metric"><span class="label">News articles</span><b>'+articles+'</b></div><div class="metric"><span class="label">News decisions</span><b>'+alerts+'</b></div><div class="metric"><span class="label">Missing prices</span><b>'+missing+'</b></div></div><p>All requests completed successfully. The dashboard cache refreshes every 15 seconds while a view is open.</p></div>'}catch(error){view.innerHTML='<div class="panel error">Data health check failed: '+error.message+'</div>'}};tab.onclick=showHealth;const enhance=()=>{if(!document.querySelector('.tab.active')||document.querySelector('.tab.active').dataset.tab!=='alerts')return;const table=[...view.querySelectorAll('.table')].find(x=>x.rows[0]?.cells[0]?.textContent==='#'&&x.rows[0]?.cells[1]?.textContent==='Company');if(!table)return;const saved=state();[...table.rows].slice(1).forEach((row,index)=>{if(row.dataset.alertEnhanced)return;const key='row-'+index,cell=document.createElement('td'),button=document.createElement('button');button.type='button';button.textContent=saved[key]?'Acknowledged':'Acknowledge';button.style.cssText='padding:6px 9px;border:1px solid var(--teal);background:var(--panel);color:var(--teal);cursor:pointer';button.onclick=()=>{const next=state();if(next[key])delete next[key];else next[key]=Date.now();save(next);button.textContent=next[key]?'Acknowledged':'Acknowledge'};cell.appendChild(button);row.appendChild(cell);row.dataset.alertEnhanced='1'});if(table.rows[0].cells[table.rows[0].cells.length-1]?.textContent!=='Status'){const header=document.createElement('th');header.textContent='Status';table.rows[0].appendChild(header)}};new MutationObserver(enhance).observe(view,{childList:true,subtree:true})})()</script>)JS";
}

const char* dashboardRefreshGuard() {
    return R"JS(<script>document.querySelector('.tabs')?.addEventListener('click',event=>{const tab=event.target.closest('.tab');if(tab&&tab.dataset.tab==='health')activeTab='config'},true)</script>)JS";
}

std::string dashboardVersion() {
    return std::string("<script>document.querySelector('.kicker').insertAdjacentHTML('beforeend',' <span style=\"font-size:10px;letter-spacing:1px;color:var(--muted)\">v") +
           PORTFOLIO_HEALTH_VERSION + "</span>');</script>";
}

const char* categoryEnhancements() {
    return R"JS(<script>(function(){const categoryOrder=['Neutral news','No recent news','going good','invest more','sell it off'];function normalizeLabel(value){const label=String(value||'').trim();const map={"neutral news":"Neutral news","no recent news":"No recent news","going good":"going good","invest more":"invest more","sell it off":"sell it off"};return map[label.toLowerCase()]||label||'Neutral news'};const render=()=>{if(document.querySelector('.tab.active')?.dataset.tab!=='deeper-analysis'||document.querySelector('.deeper-categories')||!window.deeperCategories)return;const data=window.deeperCategories;const orderedLabels=categoryOrder.filter(label=>Object.prototype.hasOwnProperty.call(data.counts||{},label)||Object.prototype.hasOwnProperty.call(data.stocks||{},label));const labels=orderedLabels.length?orderedLabels:(Object.keys(data.counts||{}).map(normalizeLabel));const panel=document.createElement('section');panel.className='panel deeper-categories';panel.innerHTML='<div class="label">News categorization</div><div style="display:flex;gap:8px;flex-wrap:wrap;margin-top:12px">'+labels.map(label=>'<button type="button" style="padding:10px 14px;border:1px solid #0d7774;background:#fffdf8;color:#172126;cursor:pointer"><b>'+String(data.counts?.[label]??0)+'</b> '+label+'</button>').join('')+'</div><p class="category-stocks" style="margin-bottom:0">Select a category to see its stocks.</p>';panel.querySelectorAll('button').forEach((button,index)=>button.onclick=()=>{const label=labels[index],names=data.stocks?.[label]||[];panel.querySelector('.category-stocks').textContent=label+': '+(names.length?names.join(', '):'No stocks')});document.querySelector('#view').prepend(panel)};const originalFetch=window.fetch;window.fetch=async function(){const response=await originalFetch.apply(this,arguments);if(String(arguments[0]).includes('/api/deeper-analysis')){const copy=response.clone();copy.json().then(data=>{const counts={};const stocks={};const ordered=Object.keys(data.category_counts||{});for(const key of categoryOrder){const normalized=normalizeLabel(key);counts[normalized]=Number(data.category_counts?.[key]??data.category_counts?.[normalized]??0);stocks[normalized]=Array.isArray(data.category_stocks?.[key])?data.category_stocks[key]:Array.isArray(data.category_stocks?.[normalized])?data.category_stocks[normalized]:[];}for(const key of ordered){const normalized=normalizeLabel(key);if(!(normalized in counts)){counts[normalized]=Number(data.category_counts?.[key]??0);stocks[normalized]=data.category_stocks?.[key]||[]}}window.deeperCategories={counts,stocks};render();}).catch(()=>{})}return response};new MutationObserver(render).observe(document.querySelector('#view'),{childList:true});})()</script>)JS";
}

std::string releaseNotice() {
    return std::string("<script>(function(){const version='") + PORTFOLIO_HEALTH_VERSION +
           R"JS(';if(localStorage.getItem('baran-capital-view-release-seen')===version)return;const box=document.createElement('aside');box.setAttribute('role','dialog');box.setAttribute('aria-label','What is new');box.style.cssText='position:fixed;z-index:50;right:24px;top:24px;width:min(420px,calc(100% - 48px));padding:18px 20px;background:#fffdf8;color:#172126;border:1px solid #0d7774;box-shadow:0 16px 40px #17212630;font:14px/1.45 Arial,sans-serif;overflow-y:auto;max-height:90vh';box.innerHTML='<button type="button" aria-label="Close release notes" style="float:right;border:0;background:transparent;color:#6b777b;font-size:22px;line-height:1;cursor:pointer">&times;</button><div style="font-size:10px;letter-spacing:1.5px;text-transform:uppercase;color:#0d7774;font-weight:700">What is new · v)JS" +
           PORTFOLIO_HEALTH_VERSION +
           R"JS(</div><strong style="display:block;margin-top:8px;font:500 21px Georgia,serif">Dependency management + API resilience</strong><p style="margin:8px 0 0;color:#6b777b">This patch removes vendored headers, manages dependencies through Conan, fixes JSON parsing errors on long-running sessions, and optimizes all dashboard tabs.</p><h3 style="font:700 13px Arial,sans-serif;margin:12px 0 6px;color:#0d7774">Build improvements</h3><ul style="margin:6px 0 0 18px;padding:0;color:#47575d;line-height:1.6;font-size:13px"><li>Removed vendored <code>third_party/nlohmann</code> header and added Conan dependency management</li><li>Simplified CMakeLists.txt dependency resolution from 35+ lines to 2 lines</li><li>Cleaner build configuration with single source of truth in conanfile.py</li><li>Consistent nlohmann_json version across CMake and Bazel build systems</li></ul><h3 style="font:700 13px Arial,sans-serif;margin:12px 0 6px;color:#0d7774">Core fixes</h3><ul style="margin:6px 0 0 18px;padding:0;color:#47575d;line-height:1.6;font-size:13px"><li>Fixed 'unexpected character' JSON errors by validating all API responses</li><li>Added robust error handling for empty, malformed, or incomplete JSON payloads</li><li>Browser now shows detailed error messages with context instead of parse exceptions</li><li>All API endpoints include proper fallback JSON for edge cases</li></ul><h3 style="font:700 13px Arial,sans-serif;margin:12px 0 6px;color:#0d7774">UI improvements</h3><ul style="margin:6px 0 0 18px;padding:0;color:#47575d;line-height:1.6;font-size:13px"><li>Data health, JSON, and Config tabs show operational diagnostics</li><li>Error messages display in a dedicated error panel with clear, actionable guidance</li><li>Login validation: codes are trimmed, URL-decoded, and compared safely before session creation</li><li>All tabs use a single canonical render path to prevent UI conflicts and stale data</li></ul>';box.querySelector('button').onclick=()=>{localStorage.setItem('baran-capital-view-release-seen',version);box.remove()};document.body.appendChild(box)})()</script>)JS";
}

const char* sortingReleaseNotice() {
    return R"JS(<script>const releaseBox=document.querySelector('[role="dialog"]');if(releaseBox){releaseBox.insertAdjacentHTML('beforeend','<h3 style="font:700 13px Arial,sans-serif;margin:12px 0 6px;color:#0d7774">Sorting improvements</h3><p style="margin:6px 0 0;color:#47575d;font-size:13px">Added accessible up/down sorting arrows to Overview, Alerts, Deeper analysis, and Fundamentals tables. Sorting is client-side over loaded rows, preserves serial numbers, and does not make extra Stock API requests.</p>')}</script>)JS";
}

const char* page() {
    return R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Portfolio Health</title>
<style>
 :root{--ink:#172126;--muted:#6b777b;--paper:#f4f1ea;--panel:#fffdf8;--line:#ded9cf;--teal:#0d7774;--orange:#d96b3b;--red:#b74747;--green:#247a4b;--yellow:#8b6b00;--shadow:0 16px 40px #17212612}*{box-sizing:border-box}body{margin:0;background:linear-gradient(135deg,#e9f1ed,#f7eee5 55%,#ece8df);color:var(--ink);font:15px/1.5 Georgia,serif;min-height:100vh}.shell{max-width:1180px;margin:auto;padding:42px 24px}.mast{display:flex;justify-content:space-between;align-items:end;border-bottom:2px solid var(--ink);padding-bottom:22px;margin-bottom:24px}.kicker{font:700 11px/1.2 Arial,sans-serif;letter-spacing:2px;text-transform:uppercase;color:var(--teal)}h1{font-size:clamp(34px,5vw,66px);line-height:.95;margin:8px 0 0;font-weight:500;letter-spacing:0}.status{font:12px Arial,sans-serif;color:var(--muted)}.tabs{display:flex;gap:8px;flex-wrap:wrap;margin-bottom:24px}.tab{font:700 13px Arial,sans-serif;border:1px solid var(--line);background:var(--panel);color:var(--ink);padding:11px 16px;cursor:pointer}.tab.active,.tab:hover{background:var(--teal);border-color:var(--teal);color:white}.grid{display:grid;grid-template-columns:repeat(4,1fr);gap:12px;margin-bottom:24px}.metric,.panel{background:var(--panel);border:1px solid var(--line);box-shadow:var(--shadow)}.metric{padding:18px}.metric b{display:block;font-size:27px;font-weight:500;margin-top:8px}.label{font:700 10px Arial,sans-serif;letter-spacing:1.4px;text-transform:uppercase;color:var(--muted)}.panel{padding:22px}.panel h2{font-size:25px;font-weight:500;margin:0 0 14px}.panel p{color:var(--muted)}.table-wrap{overflow:auto}.table{width:100%;border-collapse:collapse;font-family:Arial,sans-serif;font-size:13px}.table th{text-align:left;color:var(--muted);font-size:10px;letter-spacing:1px;text-transform:uppercase}.table td,.table th{padding:12px 10px;border-bottom:1px solid var(--line);white-space:nowrap}.table tr:hover{background:#f0ebe1}.decision{font-weight:700}.decision-add{color:var(--green)}.decision-hold{color:var(--yellow)}.decision-risk{color:var(--orange)}.news{display:grid;grid-template-columns:repeat(2,1fr);gap:12px}.story{padding:17px;border-left:4px solid var(--orange);background:#fffaf1}.story h3{font-size:18px;line-height:1.2;margin:0 0 8px;font-weight:500}.story a{color:var(--teal);font:700 12px Arial,sans-serif;text-decoration:none}.story small{display:block;color:var(--muted);font:12px Arial,sans-serif;margin-bottom:8px}.error{color:var(--red);font-family:Arial,sans-serif}.loading{color:var(--muted);padding:28px 0}@media(max-width:760px){.shell{padding:25px 14px}.mast{display:block}.status{margin-top:16px}.grid{grid-template-columns:repeat(2,1fr)}.news{grid-template-columns:1fr}.metric{padding:13px}.metric b{font-size:21px}}
</style><style>.market-status{display:flex;align-items:center;gap:8px;margin-bottom:10px;font:700 11px Arial,sans-serif;letter-spacing:1px;text-transform:uppercase}.market-dot{width:10px;height:10px;border-radius:50%;background:#b74747;box-shadow:0 0 0 4px #b7474720}.market-status.open{color:#247a4b}.market-status.open .market-dot{background:#247a4b;box-shadow:0 0 0 4px #247a4b20}.market-status.closed{color:#b74747}.header-actions{display:flex;flex-direction:column;align-items:flex-end;gap:12px}.logout-button{border:1px solid #b74747;background:transparent;color:#b74747;padding:8px 14px;font:700 12px Arial,sans-serif;cursor:pointer}.logout-button:hover{background:#b74747;color:#fffdf8}.holding-cell{position:relative}.holding-symbol{cursor:help;border-bottom:1px dotted var(--teal)}.holding-insight{display:none;position:absolute;z-index:5;left:10px;top:calc(100% - 2px);width:330px;white-space:normal;padding:14px;background:#172126;color:#fffdf8;border:1px solid #ffffff33;box-shadow:0 14px 30px #17212645;font:13px/1.4 Arial,sans-serif}.holding-cell:hover .holding-insight,.holding-cell:focus-within .holding-insight{display:block}.holding-insight strong{display:block;color:#f4c95d;font-size:14px;margin-bottom:5px}.holding-insight small{display:block;color:#b9c5c5;margin-top:8px}.holding-insight a{color:#8ed6cc;text-decoration:none}</style></head><body><main class="shell"><header class="mast"><div><div class="market-status closed" id="market-status"><span class="market-dot"></span><span id="market-label">Market closed</span></div><div class="kicker">Som Baran Gupta / Upstox</div><h1>Portfolio health</h1></div><div class="header-actions"><div class="status" id="status">Connected workspace</div><button class="logout-button" onclick="location.href='/logout'">Logout</button></div></header>
<nav class="tabs" role="tablist"><button class="tab active" role="tab" aria-selected="true" data-tab="overview">Overview</button><button class="tab" role="tab" aria-selected="false" data-tab="news">News</button><button class="tab" role="tab" aria-selected="false" data-tab="alerts">Alerts</button><button class="tab" role="tab" aria-selected="false" data-tab="deeper-analysis">Deeper analysis</button><button class="tab" role="tab" aria-selected="false" data-tab="fundamentals">Fundamentals</button><button class="tab" role="tab" aria-selected="false" data-tab="positions">Positions</button><button class="tab" role="tab" aria-selected="false" data-tab="json">JSON</button><button class="tab" role="tab" aria-selected="false" data-tab="health">Data health</button><button class="tab" role="tab" aria-selected="false" data-tab="config">Config</button></nav><div class="command-bar" style="display:flex;align-items:end;gap:10px;flex-wrap:wrap;margin:-10px 0 24px;font-family:Arial,sans-serif"><label class="search-box" style="display:grid;gap:4px;flex:1 1 220px"><span class="label">Find</span><input id="view-filter" type="search" placeholder="Search this view" autocomplete="off" style="width:100%;padding:9px 11px;border:1px solid var(--line);background:var(--panel);color:var(--ink)"></label><button class="command-button" id="refresh-view" title="Refresh current view" style="padding:9px 13px;border:1px solid var(--teal);background:var(--teal);color:white;cursor:pointer">Refresh</button><button class="command-button" id="pause-refresh" title="Pause automatic refresh" style="padding:9px 13px;border:1px solid var(--line);background:var(--panel);color:var(--ink);cursor:pointer">Pause updates</button></div>
<section id="view"></section></main><script>
const view=document.querySelector('#view'),status=document.querySelector('#status');let cache={},activeTab='overview',refreshTimer,confidenceOrder='desc',refreshInFlight=false;document.head.insertAdjacentHTML('beforeend','<style>.tabs{gap:6px;padding:6px;background:#dfe9e5;border:1px solid #cbd8d3;box-shadow:inset 0 1px 2px #17212612}.tab{border:0;border-radius:3px;background:transparent;color:#385157;padding:10px 15px;transition:background .18s ease,color .18s ease,transform .18s ease;position:relative}.tab:hover{background:#f8fbf8;color:#0d7774;transform:translateY(-1px)}.tab.active{background:#0d7774;color:#fff;box-shadow:0 4px 10px #0d777433}.tab:focus-visible{outline:2px solid #d96b3b;outline-offset:2px}.command-bar{background:#eef4f1;padding:10px 12px;border:1px solid #d7e2dd}.command-button{border-radius:3px;transition:filter .18s ease,transform .18s ease}.command-button:hover{filter:brightness(.95);transform:translateY(-1px)}.metric,.panel{transition:box-shadow .2s ease,border-color .2s ease}.metric:hover,.panel:hover{border-color:#b7cbc4;box-shadow:0 12px 28px #1721261c}.market-status{position:relative;cursor:help}.market-status:hover::after,.market-status:focus-visible::after{content:attr(data-market-message);position:absolute;z-index:40;left:0;top:calc(100% + 8px);width:190px;padding:9px 11px;background:#172126;color:#fffdf8;border:1px solid #ffffff33;box-shadow:0 10px 24px #17212645;font:12px/1.45 Arial,sans-serif;text-transform:none;letter-spacing:0;pointer-events:none}.market-status:hover::before,.market-status:focus-visible::before{content:"Market hours";position:absolute;z-index:41;left:0;top:calc(100% + 8px);transform:translateY(-1px);padding:9px 11px;color:#f4c95d;font:700 12px/1.45 Arial,sans-serif;pointer-events:none}body.market-open{background:linear-gradient(135deg,#dcefe1,#eef5e9 55%,#e5f0e5)}body.market-closed{background:linear-gradient(135deg,#f2dfdc,#f7ece8 55%,#eee2df)}.workflow-reference{background:#151a2a;color:#edf1fb;padding:26px 30px;margin-bottom:24px;border:1px solid #2b344d;box-shadow:0 16px 35px #17212625;font-family:Arial,sans-serif}.workflow-reference h2{font-size:25px;margin:4px 0 18px;color:#fff}.workflow-kicker{color:#94a8d1;font-size:11px;font-weight:700;letter-spacing:1.5px;text-transform:uppercase}.workflow-reference ol{margin:0;padding-left:28px}.workflow-reference li{padding:7px 0 7px 8px;font-size:14px;line-height:1.4}.workflow-reference li::marker{color:#93a6cf;font-weight:700}.workflow-reference li b{display:block;color:#f4f6ff}.workflow-reference li span{display:block;color:#aeb8cf;font-size:12px;margin-top:2px}.deep-loading{display:grid;place-items:center;gap:14px;min-height:180px;text-align:center;font-family:Arial,sans-serif}.deep-spinner{width:34px;height:34px;border:4px solid #cbd8d3;border-top-color:var(--teal);border-right-color:var(--orange);border-radius:50%;animation:deep-spin .8s linear infinite}@keyframes deep-spin{to{transform:rotate(360deg)}}.analysis-number{color:var(--teal);font-weight:700}.analysis-table td:first-child{width:42px;text-align:center}.analysis-table tbody tr{animation:analysis-rise .3s ease both}@keyframes analysis-rise{from{opacity:0;transform:translateY(5px)}to{opacity:1;transform:none}}@media(max-width:650px){.tab{flex:1 1 auto;text-align:center;padding:9px 10px;font-size:12px}.workflow-reference{padding:22px 18px}.workflow-reference li{font-size:13px}}</style>');
function sortByConfidence(items){return [...items].sort((a,b)=>{const difference=Number(a.confidence||0)-Number(b.confidence||0);return confidenceOrder==='asc'?difference:-difference})}
function sortOverviewRows(){if(activeTab!=='overview'||!cache.holdings||!cache.news)return;const confidence={};(cache.news.alerts||[]).forEach(x=>confidence[x.instrument_key]=Number(x.confidence||0));const bySymbol={};(cache.holdings.data||[]).forEach(x=>bySymbol[x.trading_symbol||x.tradingsymbol]=confidence[x.instrument_token]||0);const table=document.querySelector('.table');if(!table)return;const rows=[...table.querySelectorAll('tr')].slice(1);rows.sort((a,b)=>{const difference=(bySymbol[b.querySelector('.holding-symbol')?.textContent.trim()]||0)-(bySymbol[a.querySelector('.holding-symbol')?.textContent.trim()]||0);return confidenceOrder==='asc'?-difference:difference});const body=table.tBodies[0]||table;rows.forEach(row=>body.appendChild(row))}
function changeConfidenceOrder(value){confidenceOrder=value;cache={};openTab(activeTab,false).then(()=>{if(activeTab==='overview')sortOverviewRows()})}
function updateMarketStatus(){const now=new Date(),parts=new Intl.DateTimeFormat('en-IN',{timeZone:'Asia/Kolkata',weekday:'short',hour:'2-digit',minute:'2-digit',hour12:false}).formatToParts(now),get=k=>parts.find(x=>x.type===k)?.value,day=get('weekday'),minutes=Number(get('hour'))*60+Number(get('minute')),open=!['Sat','Sun'].includes(day)&&minutes>=555&&minutes<930,box=document.querySelector('#market-status'),label=document.querySelector('#market-label');box.className='market-status '+(open?'open':'closed');box.tabIndex=0;label.textContent=open?'Market open':'Market closed';box.dataset.marketMessage=open?'Closes at 15:30 IST':'Opens at 09:15 IST';document.body.classList.toggle('market-open',open);document.body.classList.toggle('market-closed',!open)}
function esc(v){return String(v??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]))}
async function get(name,url){if(cache[name])return cache[name];let r=await fetch(url);if(!r.ok){if(name==='news'){status.textContent='News unavailable · retrying automatically';return cache[name]={data:{},alerts:[]}}throw Error('HTTP '+r.status)};try{return cache[name]=await r.json()}catch(e){console.error('JSON parse error for '+name+' from '+url+':',e);if(name==='news')return cache[name]={data:{},alerts:[]};throw Error('Invalid JSON response from '+name)}}
async function safeJsonParse(response,fallback){try{const text=await response.text();if(!text||!text.trim())return fallback||{status:'error',error:'Empty response'};return JSON.parse(text)}catch(error){console.warn('Malformed JSON response',error);return fallback||{status:'error',error:String(error && error.message ? error.message : error)}}}
async function safeJsonFetch(url,fallback){const effectiveFallback=fallback&&typeof fallback==='object'?fallback:{status:'error',error:'Request failed'};try{const response=await fetch(url,{cache:'no-store'});if(!response||!response.ok){return {...effectiveFallback,status:'error',error:effectiveFallback.error||('HTTP '+(response?response.status:'request failed'))}};return await safeJsonParse(response,effectiveFallback)}catch(error){console.warn('safeJsonFetch failed',url,error);return {...effectiveFallback,status:'error',error:String(error && error.message ? error.message : error)}}}
function filterView(value){const query=value.trim().toLowerCase();view.querySelectorAll('tr,article,.story,.fundamental-stock').forEach(item=>{const match=!query||item.textContent.toLowerCase().includes(query);item.hidden=!match})}
function sortableValue(value){const text=String(value||'').trim();const numeric=Number(text.replace(/[INR,%+]/g,'').replace(/,/g,''));return text&&Number.isFinite(numeric)?numeric:text.toLowerCase()}
function sortTable(table,index,direction){const body=table.tBodies[0];if(!body)return;const header=table.tHead?.rows[0]||table.rows[0],rows=[...body.rows].filter(row=>row!==header);rows.sort((left,right)=>{const a=sortableValue(left.cells[index]?.textContent),b=sortableValue(right.cells[index]?.textContent);if(a===b)return Number(left.dataset.sortIndex||0)-Number(right.dataset.sortIndex||0);if(typeof a==='number'&&typeof b==='number')return direction*(a-b);return direction*String(a).localeCompare(String(b),undefined,{numeric:true,sensitivity:'base'})});rows.forEach(row=>body.appendChild(row));addSerialNumbers()}
function enhanceSortableTables(){if(!['overview','alerts','deeper-analysis','fundamentals'].includes(activeTab))return;view.querySelectorAll('table.table').forEach(table=>{if(table.dataset.sortableReady)return;const header=table.tHead?.rows[0]||table.rows[0];if(!header||header.cells.length<2)return;const rows=table.tBodies[0]?.rows||[...table.rows].slice(1);[...rows].forEach((row,index)=>row.dataset.sortIndex=String(index));[...header.cells].forEach((cell,index)=>{const label=cell.textContent.trim();if(!label||label==='#')return;const button=document.createElement('button');button.type='button';button.className='sort-control';button.innerHTML='<span>'+esc(label)+'</span><span class="sort-arrows" aria-hidden="true">&uarr;&darr;</span>';button.setAttribute('aria-label','Sort by '+label);cell.textContent='';cell.appendChild(button);let direction=-1;button.onclick=()=>{direction*=-1;table.querySelectorAll('.sort-control').forEach(item=>{item.removeAttribute('data-sort-direction');item.setAttribute('aria-label','Sort by '+item.querySelector('span')?.textContent)});button.dataset.sortDirection=direction===1?'ascending':'descending';button.setAttribute('aria-label','Sort by '+label+' '+(direction===1?'ascending':'descending'));sortTable(table,index,direction)}});table.dataset.sortableReady='1'})}
function numberField(item,keys){for(const key of keys){const value=Number(item[key]);if(Number.isFinite(value))return value}return 0}
function holdingValue(item){const reported=numberField(item,['current_value','market_value','value']);if(reported)return reported;return numberField(item,['last_price'])*numberField(item,['quantity'])*Math.max(1,numberField(item,['multiplier'])||1)}
function stockCount(items){return new Set(items.map(item=>(item.trading_symbol||item.tradingsymbol||'').trim()).filter(Boolean)).size}
function refreshView(){if(refreshInFlight)return;cache={};refreshInFlight=true;openTab(activeTab,false).finally(()=>refreshInFlight=false)}
let updatesPaused=false;
function toggleUpdates(){updatesPaused=!updatesPaused;document.querySelector('#pause-refresh').textContent=updatesPaused?'Resume updates':'Pause updates';document.querySelector('#pause-refresh').setAttribute('aria-pressed',String(updatesPaused))}
function fail(e){const msg=e&&e.message?e.message:String(e);view.innerHTML='<div class="panel error"><div class="label">Error details</div><h2>Unable to load this view</h2><p>'+esc(msg)+'</p><p style="color:var(--muted);font-size:12px">Check the browser console for details. Try refreshing the page.</p></div>';status.textContent='Error: '+esc(msg)}
function rows(data){return Object.entries(data).flatMap(([key,items])=>items.map(x=>({...x,instrument_key:key}))) }
function decisionSummary(action){const normalized=(action||'').trim();if(!normalized)return 'Hold / wait';const lower=normalized.toLowerCase();if(lower.includes('consider'))return 'Consider adding';if(lower.includes('do not'))return 'Do not add';if(lower.includes('hold'))return 'Hold / wait';if(lower.includes('buy'))return 'Buy / review';if(lower.includes('sell'))return 'Sell / review';return normalized}
function normalizeCategoryLabel(value){const label=String(value||'').trim();const map={'neutral news':'Neutral news','no recent news':'No recent news','going good':'going good','invest more':'invest more','sell it off':'sell it off'};return map[label.toLowerCase()]||label||'Neutral news'}
async function holdings(){const p=await get('holdings','/api/holdings');const n=await get('news','/api/news');const d=(p.data||[]).slice();const alerts=n.alerts||[];const byKey={};for(const x of alerts){byKey[x.instrument_key]=x;}d.sort((a,b)=>Number(byKey[b.instrument_token]?.confidence||0)-Number(byKey[a.instrument_token]?.confidence||0));if(confidenceOrder==='asc')d.reverse();const value=d.reduce((sum,x)=>sum+holdingValue(x),0);status.textContent=d.length+' live holdings · updated '+new Date().toLocaleTimeString();view.innerHTML='<div class="grid"><div class="metric"><span class="label">Holdings</span><b>'+d.length+'</b></div><div class="metric"><span class="label">Market value</span><b>INR '+value.toLocaleString('en-IN',{maximumFractionDigits:0})+'</b></div><div class="metric"><span class="label">Day P&amp;L</span><b>'+d.reduce((sum,x)=>sum+(x.day_change||0)*(x.quantity||0),0).toLocaleString('en-IN',{maximumFractionDigits:0})+'</b></div><div class="metric"><span class="label">Refresh</span><b>30 sec</b></div></div><div class="panel"><h2>Long-term holdings</h2><label class="label">Confidence order <select onchange="changeConfidenceOrder(this.value)"><option value="desc"'+(confidenceOrder==='desc'?' selected':'')+'>Highest first</option><option value="asc"'+(confidenceOrder==='asc'?' selected':'')+'>Lowest first</option></select></label><p>Hover over a stock to see what recent company news may mean. News is context, not a forecast.</p><div class="table-wrap"><table class="table"><tr><th>Symbol</th><th>Quantity</th><th>Average</th><th>Last</th><th>P&amp;L</th></tr>'+d.map(x=>{const key=x.instrument_token||'';const alert=byKey[key]||{};const items=(n.data||{})[key]||[];const article=items[0];const action=alert.action||'No recent news';const reason=alert.rationale||'No matching news in the recent feed';return '<tr><td class="holding-cell"><span class="holding-symbol" tabindex="0"><b>'+esc(x.trading_symbol||x.tradingsymbol)+'</b></span><span class="holding-insight"><strong>'+esc(decisionSummary(action))+'</strong>'+esc(reason)+(article?'<small>Latest: '+esc(article.heading)+'</small><a href="'+esc(article.article_link)+'" target="_blank" rel="noopener">Read related news</a>':'')+'</span></td><td>'+x.quantity+'</td><td>'+((x.average_price===undefined||x.average_price===null)?'—':x.average_price)+'</td><td>'+((x.last_price===undefined||x.last_price===null)?'—':x.last_price)+'</td><td>'+((x.pnl===undefined||x.pnl===null)?'—':x.pnl)+'</td></tr>'}).join('')+'</table></div></div>'}
async function news(){let p=await get('news','/api/news'),a=rows(p.data||{}).sort((left,right)=>Number(right.published_time||0)-Number(left.published_time||0));status.textContent=a.length+' articles from holding.csv · newest first';view.innerHTML='<div class="panel"><h2>News for your holdings</h2><p>Articles matched to the symbols in <b>holding.csv</b>, newest first.</p><div class="news">'+a.map(x=>'<article class="story"><small>'+esc(x.instrument_key)+'</small><h3>'+esc(x.heading)+'</h3><p>'+esc(x.summary||'')+'</p><a href="'+esc(x.article_link)+'" target="_blank" rel="noopener">Read article &rarr;</a></article>').join('')+'</div></div>'}
async function alerts(){let p=await get('news','/api/news'),h=await get('holdings','/api/holdings'),byKey={};(p.alerts||[]).forEach(x=>byKey[x.instrument_key]=x);let a=(h.data||[]).map(x=>{let key=x.instrument_token||'',alert=byKey[key]||{},items=(p.data||{})[key]||[],article=items[0];return {...alert,instrument_key:key,company_name:x.company_name||x.trading_symbol||x.tradingsymbol||'Unknown company',article_count:items.length,article_link:article?.article_link||''}});a=sortByConfidence(a);status.textContent=a.length+' stocks in portfolio · updated '+new Date().toLocaleTimeString();view.innerHTML='<div class="panel"><h2>Should I add more?</h2><p>Every holding is shown, including stocks without recent matching news. Each row combines sentiment, article agreement, and recency. It is a review prompt, not an automatic trade.</p><label class="label">Confidence order <select onchange="changeConfidenceOrder(this.value)"><option value="desc"'+(confidenceOrder==='desc'?' selected':'')+'>Highest first</option><option value="asc"'+(confidenceOrder==='asc'?' selected':'')+'>Lowest first</option></select></label><div class="grid"><div class="metric"><span class="label">Consider adding</span><b>'+a.filter(x=>(x.action||'').startsWith('Consider')).length+'</b></div><div class="metric"><span class="label">Risk review</span><b>'+a.filter(x=>(x.action||'').startsWith('Do not')).length+'</b></div><div class="metric"><span class="label">Hold / wait</span><b>'+a.filter(x=>(x.action||'').startsWith('Hold')||!x.action).length+'</b></div><div class="metric"><span class="label">Total stocks</span><b>'+a.length+'</b></div></div><p><b>Consider adding</b> means positive news deserves review. <b>Do not add</b> means negative news deserves risk review. <b>Hold / wait</b> means the signal is mixed or not confident enough. Always read the articles and check valuation before acting.</p><div class="table-wrap"><table class="table"><tr><th>#</th><th>Company</th><th>News score</th><th>Confidence</th><th>Decision</th><th>Why</th><th>Articles</th></tr>'+a.map((x,index)=>'<tr><td>'+String(index+1)+'</td><td>'+esc(x.company_name)+'</td><td>'+Number(x.sentiment_score||0).toFixed(2)+'</td><td>'+Math.round(Number(x.confidence||0)*100)+'%</td><td><span class="decision '+((x.action||'').startsWith('Consider')?'decision-add':(x.action||'').startsWith('Do not')?'decision-risk':'decision-hold')+'">'+esc(x.action||'Hold / wait')+'</span></td><td>'+esc(x.rationale||'No matching news in the recent feed.')+(x.article_link?' <a href="'+esc(x.article_link)+'" target="_blank" rel="noopener">View news</a>':'')+'</td><td>'+x.article_count+'</td></tr>').join('')+'</table></div></div>'}
async function raw(name,url,title){let p=await get(name,url);status.textContent='Live API response';view.innerHTML='<div class="panel"><div class="label">Runtime payload</div><h2>'+title+'</h2><p>Read only. This view exposes the raw server payload for the selected endpoint without modifying the stock API configuration.</p><pre style="white-space:pre-wrap;overflow:auto;font:12px/1.5 monospace">'+esc(JSON.stringify(p,null,2))+'</pre></div>'}
async function health(){const [holdingsData, newsData] = await Promise.all([get('holdings','/api/holdings'), get('news','/api/news')]);const rows = holdingsData.data || [];const articles = Object.values(newsData.data || {}).reduce((sum, items) => sum + (Array.isArray(items) ? items.length : 0), 0);const alerts = (newsData.alerts || []).length;const missing = rows.filter(item => Number(item.last_price ?? item.lastPrice ?? 0) <= 0).length;status.textContent='Data health checked · '+new Date().toLocaleTimeString();view.innerHTML='<div class="panel"><div class="label">Runtime diagnostics</div><h2>Data health</h2><p>Operational readout for the current portfolio snapshot, filtered news set, and alert generation state.</p><div class="grid"><div class="metric"><span class="label">Holdings loaded</span><b>'+rows.length+'</b></div><div class="metric"><span class="label">News articles</span><b>'+articles+'</b></div><div class="metric"><span class="label">News decisions</span><b>'+alerts+'</b></div><div class="metric"><span class="label">Missing prices</span><b>'+missing+'</b></div></div><div class="table-wrap"><table class="table"><tr><th>Check</th><th>Status</th><th>Notes</th></tr><tr><td>Portfolio snapshot</td><td class="decision '+(rows.length?'decision-add':'decision-hold')+'">'+(rows.length?'Healthy':'Empty')+'</td><td>Requested holdings payload loaded from the current Upstox snapshot.</td></tr><tr><td>News filtering</td><td class="decision '+(articles?'decision-add':'decision-hold')+'">'+(articles?'Active':'Quiet')+'</td><td>Recent articles are filtered to the configuration-backed holdings list.</td></tr><tr><td>Alert generation</td><td class="decision '+(alerts?'decision-add':'decision-hold')+'">'+(alerts?'Generated':'Idle')+'</td><td>Alert decisions are computed from filtered sentiment, not stale browser state.</td></tr></table></div></div>'}
async function config(){view.innerHTML='<div class="panel"><div class="label">Secure runtime settings</div><h2>Configuration</h2><p>Stock API traffic remains server-side only. Secrets are loaded from the environment and never exposed to the browser.</p><div class="grid"><div class="metric"><span class="label">Holdings source</span><b>config/holding.csv</b></div><div class="metric"><span class="label">News source</span><b>config/portfolio_news.json</b></div><div class="metric"><span class="label">Auth mode</span><b>Env + session</b></div><div class="metric"><span class="label">Security model</span><b>HTTPS only</b></div></div><p><strong>Operational guidance:</strong> this page summarises the approved configuration boundary, the file-based holdings filter, and the secure session secret path used for access control.</p></div>'}
async function deeperAnalysis(){const p=await safeJsonFetch('/api/deeper-analysis',{status:'error',error:'Deeper analysis is unavailable right now.'});if(activeTab!=='deeper-analysis')return;if(p.status==='running'){status.textContent='Deeper analysis is running...';view.innerHTML='<div class="panel loading"><h2>Deeper analysis</h2><p>Comparing saved and fresh news. Other tabs remain available.</p></div>';return}if(p.status==='error')throw Error(p.error);let s=p.stocks||[];status.textContent=s.length+' stocks compared · '+new Date().toLocaleTimeString();view.innerHTML='<div class="panel"><div style="border-bottom:1px solid var(--line);padding-bottom:18px;margin-bottom:20px"><div class="label">Portfolio intelligence</div><h2 style="font-size:32px;margin:6px 0 10px">How this ties to NLP sentiment</h2><div style="display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:10px;font-family:Arial,sans-serif;font-size:13px"><div style="padding:12px 14px;background:#f0ebe1;border-left:3px solid var(--teal)"><b>01 · Know what you own</b><br><span style="color:var(--muted)">Portfolio Holdings API identifies the stocks in your account.</span></div><div style="padding:12px 14px;background:#f0ebe1;border-left:3px solid var(--orange)"><b>02 · Track live changes</b><br><span style="color:var(--muted)">Market quotes keep price context current.</span></div><div style="padding:12px 14px;background:#f0ebe1;border-left:3px solid var(--teal)"><b>03 · Overlay sentiment</b><br><span style="color:var(--muted)">NLP scores add meaning to live price and news feeds.</span></div><div style="padding:12px 14px;background:#f0ebe1;border-left:3px solid var(--orange)"><b>04 · Trigger review</b><br><span style="color:var(--muted)">Alerts surface threshold crossings for a human decision.</span></div><div style="padding:12px 14px;background:#f0ebe1;border-left:3px solid var(--teal);grid-column:1/-1"><b>05 · Validate the strategy</b><br><span style="color:var(--muted)">Historical data supports backtesting before any sentiment-driven workflow is trusted.</span></div></div></div><h2>Deeper analysis</h2><p>Saved portfolio news is compared with a fresh Python news sentiment run. Review every signal before acting.</p><div class="table-wrap"><table class="table"><tr><th>Stock</th><th>Saved news</th><th>Python news</th><th>Analysis</th><th>Action</th></tr>'+s.map(x=>'<tr><td><b>'+esc(x.symbol)+'</b></td><td>'+esc(x.saved_signal)+'</td><td>'+esc(x.python_recommendation)+'</td><td>'+esc(x.analysis)+'</td><td class="decision">'+esc(x.action)+'</td></tr>').join('')+'</table></div></div>'}
async function openTab(tab,showLoading=true){activeTab=tab;document.querySelectorAll('.tab').forEach(x=>{const active=x.dataset.tab===tab;x.classList.toggle('active',active);x.setAttribute('aria-selected',String(active))});document.querySelector('#view-filter').value='';if(showLoading)view.innerHTML='<div class="panel loading">Loading '+tab+'...</div>';try{if(tab==='overview')await holdings();else if(tab==='news')await news();else if(tab==='alerts')await alerts();else if(tab==='deeper-analysis')await deeperAnalysis();else if(tab==='fundamentals')await fundamentals();else if(tab==='positions')await raw('positions','/api/positions','Open positions');else if(tab==='json')await raw('holdings','/api/holdings','Holdings JSON');else if(tab==='health')await health();else await config()}catch(e){fail(e)}enhanceSortableTables()}
function addSerialNumbers(){const table=document.querySelector('.table');if(!table||!table.querySelector('.holding-symbol'))return;const head=table.rows[0];if(head.cells[0]?.textContent==='#')return;const header=document.createElement('th');header.textContent='#';head.insertBefore(header,head.firstChild);[...table.rows].slice(1).forEach((row,index)=>{const cell=document.createElement('td');cell.textContent=String(index+1);row.insertBefore(cell,row.firstChild)})}
function normalizeRefreshMetric(){if(activeTab!=='overview')return;const metrics=[...view.querySelectorAll('.metric')],data=cache.holdings?.data||[],refresh=metrics.find(item=>item.querySelector('.label')?.textContent==='Refresh');if(metrics[0]?.querySelector('.label')){const label=metrics[0].querySelector('.label'),value=String(stockCount(data));if(label.textContent!=='Stocks')label.textContent='Stocks';if(metrics[0].querySelector('b').textContent!==value)metrics[0].querySelector('b').textContent=value}if(metrics[1]?.querySelector('.label')?.textContent==='Market value'){const value='INR '+data.reduce((total,item)=>total+holdingValue(item),0).toLocaleString('en-IN',{minimumFractionDigits:2,maximumFractionDigits:2});if(metrics[1].querySelector('b').textContent!==value)metrics[1].querySelector('b').textContent=value}if(refresh&&!refresh.dataset.ready){refresh.dataset.ready='1';refresh.querySelector('b').innerHTML='<button type="button" class="metric-refresh" onclick="refreshView()">Refresh now</button>'}}
function normalizeDeeperAnalysis(){if(activeTab!=='deeper-analysis')return;const loading=view.querySelector('.loading');if(loading&&!loading.querySelector('.deep-spinner'))loading.innerHTML='<span class="deep-spinner" aria-hidden="true"></span><b>Analysis in progress</b><span>Comparing saved and fresh sentiment data...</span>';const table=[...view.querySelectorAll('.table')].find(item=>item.textContent.includes('Saved news')&&item.textContent.includes('Python news'));if(!table||table.rows[0].cells[0]?.textContent==='#')return;const header=document.createElement('th');header.textContent='#';table.rows[0].insertBefore(header,table.rows[0].firstChild);[...table.rows].slice(1).forEach((row,index)=>{const cell=document.createElement('td');cell.className='analysis-number';cell.textContent=String(index+1);row.insertBefore(cell,row.firstChild)})}
function normalizeAlertsTable(){const table=[...view.querySelectorAll('.table')].find(item=>item.rows[0]?.cells[0]?.textContent==='Stock'&&item.rows[0]?.cells[1]?.textContent==='Company');if(!table||table.dataset.stockRemoved)return;[...table.rows].forEach(row=>row.deleteCell(0));table.dataset.stockRemoved='1'}
function addWorkflowReference(){if(activeTab!=='deeper-analysis'||document.querySelector('.workflow-reference'))return;const legacy=[...view.querySelectorAll('h2')].find(item=>item.textContent==='How this ties to NLP sentiment');if(legacy)legacy.parentElement.remove();const panel=document.createElement('section');panel.className='workflow-reference';panel.innerHTML='<div class="workflow-kicker">Sombaran portfolio intelligence</div><h2>From market data to a decision</h2><ol><li><b>Use Upstox Market Data API</b><span>Live quotes and historical candles provide the market context.</span></li><li><b>Feed data into the analytics engine</b><span>C++ and Python process technical indicators and ML signals.</span></li><li><b>Generate buy, sell, or hold signals</b><span>Sentiment and portfolio data become reviewable actions.</span></li><li><b>Automate execution safely</b><span>Orders API integration belongs behind explicit risk controls.</span></li><li><b>Monitor portfolio health</b><span>Holdings API keeps ownership and exposure visible.</span></li><li><b>Visualize results with alerts</b><span>Dashboards and alerts make changes easy to spot.</span></li></ol></section>';view.prepend(panel)}
document.head.insertAdjacentHTML('beforeend','<style>.market-status:hover::before,.market-status:focus-visible::before{display:none!important}.market-status:hover::after,.market-status:focus-visible::after{content:"Market hours: " attr(data-market-message);white-space:nowrap}</style>');
function articleList(items){return (items||[]).map(x=>'<li><a href="'+esc(x.article_link||x.url||'#')+'" target="_blank" rel="noopener">'+esc(x.heading||x.title||'Untitled article')+'</a><small>'+esc(x.summary||x.description||'')+'</small></li>').join('')||'<li>No articles returned.</li>'}
async function showStockAnalysis(symbol){const existing=document.querySelector('.stock-detail');if(existing)existing.remove();view.insertAdjacentHTML('beforeend','<div class="panel stock-detail"><h2>'+esc(symbol)+'</h2><p class="loading">Loading news and fundamentals...</p></div>');try{const data=await safeJsonFetch('/api/stock-analysis?symbol='+encodeURIComponent(symbol),{status:'error',error:'Stock analysis is unavailable right now.'});if(activeTab!=='overview')return;if(data.status==='running'){setTimeout(()=>{if(activeTab==='overview')showStockAnalysis(symbol)},1000);return}if(data.error)throw Error(data.error);const detail=document.querySelector('.stock-detail');if(!detail)return;const ratios=(data.key_ratios||[]).map(x=>'<tr><td>'+esc(x.name)+'</td><td>'+esc(x.company_value)+'</td><td>'+esc(x.sector_value)+'</td></tr>').join('');const profile=data.company_profile||{};detail.innerHTML='<h2>'+esc(data.symbol)+' <span class="decision">'+esc(data.action)+'</span></h2><p>'+esc(data.comparison)+'</p><div class="panel"><span class="label">Fundamentals · '+esc(data.fundamentals_signal)+'</span><p>'+esc(data.fundamentals_summary)+(profile.sector?' Sector: '+esc(profile.sector)+'.':'')+'</p><div class="table-wrap"><table class="table"><tr><th>Ratio</th><th>Company</th><th>Sector</th></tr>'+ratios+'</table></div></div><div class="news"><div><span class="label">Upstox news · '+esc(data.upstox_signal)+'</span><ul class="source-list">'+articleList(data.upstox_news)+'</ul></div><div><span class="label">News-config news · '+esc(data.news_config_recommendation)+'</span><ul class="source-list">'+articleList(data.news_config_news)+'</ul></div></div>'}catch(error){const detail=document.querySelector('.stock-detail');if(detail)detail.innerHTML='<h2>'+esc(symbol)+'</h2><p class="error">Unable to complete stock analysis: '+esc(error.message)+'</p>'}}
async function fundamentals(){let p=await get('holdings','/api/holdings'),d=p.data||[];status.textContent='Select a holding for balance sheet, income, cash flow and corporate actions';view.innerHTML='<div class="panel"><h2>Sombaran Portfolio fundamentals</h2><p>Choose a holding to load its Upstox fundamentals by ISIN.</p><div class="table-wrap"><table class="table"><tr><th>#</th><th>Stock</th><th>Quantity</th><th>Last price</th><th>Open</th></tr>'+d.map((x,i)=>{const symbol=x.trading_symbol||x.tradingsymbol;return '<tr><td>'+String(i+1)+'</td><td><button class="fundamental-stock" data-symbol="'+esc(symbol)+'">'+esc(symbol)+'</button></td><td>'+x.quantity+'</td><td>'+x.last_price+'</td><td><button class="fundamental-open" data-symbol="'+esc(symbol)+'">View fundamentals</button></td></tr>'}).join('')+'</table></div></div>'}
async function showFundamentals(symbol){const existing=document.querySelector('.fundamental-detail');if(existing)existing.remove();view.insertAdjacentHTML('beforeend','<div class="panel fundamental-detail"><h2>'+esc(symbol)+'</h2><p class="loading">Loading fundamentals...</p></div>');try{const data=await safeJsonFetch('/api/fundamentals?symbol='+encodeURIComponent(symbol),{status:'error',error:'Fundamentals are unavailable right now.'});if(activeTab!=='fundamentals')return;if(data.status==='running'){setTimeout(()=>{if(activeTab==='fundamentals')showFundamentals(symbol)},1000);return}if(data.error)throw Error(data.error);const latest=(section,key)=>{const history=(section&&section[key])||section?.history||[];return history.slice(0,4).map(x=>'<tr><td>'+esc(x.period)+'</td><td>'+esc(x.value??x.total_asset??'')+'</td><td>'+esc(x.change||'')+'</td></tr>').join('')||'<tr><td colspan="3">No data</td></tr>'};const actions=(data.corporate_actions||[]).map(x=>'<tr><td>'+esc(x.name)+'</td><td>'+esc(x.expiry_date||'')+'</td><td>'+esc(x.amount??x.ratio??'')+'</td></tr>').join('')||'<tr><td colspan="3">No corporate actions</td></tr>';document.querySelector('.fundamental-detail').innerHTML='<h2>'+esc(data.symbol)+' <span class="label">'+esc(data.isin)+'</span></h2><div class="grid"><div class="panel"><h3>Balance sheet</h3><table class="table"><tr><th>Period</th><th>Assets</th><th>Change</th></tr>'+latest(data.balance_sheet,'history')+'</table></div><div class="panel"><h3>Income statement</h3><table class="table"><tr><th>Period</th><th>Value</th><th>Change</th></tr>'+latest(data.income_statement,'income_statement')+'</table></div><div class="panel"><h3>Cash flow</h3><table class="table"><tr><th>Period</th><th>Value</th><th>Change</th></tr>'+latest(data.cash_flow,'cash_flow')+'</table></div><div class="panel"><h3>Corporate actions</h3><table class="table"><tr><th>Action</th><th>Date</th><th>Amount/ratio</th></tr>'+actions+'</table></div></div>'}catch(error){const detail=document.querySelector('.fundamental-detail');if(detail)detail.innerHTML='<h2>'+esc(symbol)+'</h2><p class="error">Unable to load fundamentals: '+esc(error.message)+'</p>'}}
function fundamentalValue(value){if(value===null||value===undefined||value==='')return '-';if(typeof value==='object')return JSON.stringify(value);return String(value)}
function fundamentalRows(value){if(!value||typeof value!=='object')return '<span class="fundamental-empty">No data</span>';return Object.entries(value).map(([key,item])=>'<div class="fundamental-field"><span>'+esc(key.replace(/_/g,' '))+'</span><b>'+esc(fundamentalValue(item))+'</b></div>').join('')}
function fundamentalHistory(section){const history=Array.isArray(section)?section:(section?.history||[]);if(!history.length)return '<span class="fundamental-empty">No data</span>';return '<div class="fundamental-history">'+history.map(item=>'<div class="fundamental-row">'+fundamentalRows(item)+'</div>').join('')+'</div>'}
function ratioInterpretation(item){const name=(item.name||item.key||'ratio').toLowerCase(),company=Number(item.company_value),sector=Number(item.sector_value);if(!Number.isFinite(company)||!Number.isFinite(sector))return ['OK','No sector comparison is available.'];const lowerIsBetter=/(p\/e|p\/b|price|debt|leverage|ev\/ebitda)/.test(name),difference=company-sector;if(Math.abs(difference)<0.01)return ['OK','The company is close to its sector level.'];if(lowerIsBetter)return difference<0?['Good','Lower than the sector level.']:['Bad','Higher than the sector level.'];return difference>0?['Good','Higher than the sector level.']:['Bad','Lower than the sector level.']}
function addFundamentalSummary(){const popup=document.querySelector('.fundamental-popup');if(!popup||popup.querySelector('.fundamental-summary')||popup.querySelector('.loading'))return;const rows=[['Profile','Basic company identity and classification','OK','This describes the company, not its quality.'],['Balance sheet','Shows what the company owns and owes','OK','Review assets, liabilities, and their trend together.'],['Income statement','Shows revenue, costs, and profit over time','OK','Look for steady growth and improving profit.'],['Cash flow','Shows whether business operations generate cash','OK','Positive, growing operating cash flow is generally healthier.'],['Corporate actions','Shows dividends, splits, bonuses, or similar events','OK','An event needs context before it can be called good or bad.']].map(item=>'<tr><td><b>'+item[0]+'</b></td><td>'+item[1]+'</td><td><b>'+item[2]+'</b></td><td>'+item[3]+'</td></tr>');[...popup.querySelectorAll('.fundamental-ratio')].forEach(item=>{const cells=[...item.children],ratio={name:cells[0]?.textContent||'Ratio',company_value:(cells[1]?.textContent||'').replace(/^Company:\s*/,'').trim(),sector_value:(cells[2]?.textContent||'').replace(/^Sector:\s*/,'').trim()},result=ratioInterpretation(ratio);rows.splice(1,0,'<tr><td><b>'+esc(ratio.name)+'</b></td><td>Company value compared with sector value.</td><td><b>'+result[0]+'</b></td><td>'+result[1]+'</td></tr>')});popup.insertAdjacentHTML('afterbegin','<section class="fundamental-summary" style="grid-column:1/-1;margin-bottom:10px"><h3>Summary in plain language</h3><table class="table fundamental-summary-table" style="white-space:normal;width:100%"><tr><th>Parameter</th><th>What it means</th><th>View</th><th>One-line reason</th></tr>'+rows.join('')+'</table></section>')}
function addWorkflowReference(){}
function addRsiTool(){if(activeTab!=='deeper-analysis'||document.querySelector('.rsi-tool'))return;const tool=document.createElement('section');tool.className='panel rsi-tool';tool.innerHTML='<div class="label">Technical indicator</div><h2>RSI momentum check</h2><p>RSI compares recent upward and downward price movement. Use closing prices in time order: oldest first, latest last.</p><div style="display:flex;gap:10px;align-items:end;flex-wrap:wrap;font-family:Arial,sans-serif"><label style="flex:1 1 320px"><span class="label">Closing prices</span><input id="rsi-closes" type="text" inputmode="decimal" aria-describedby="rsi-hint" placeholder="100,101,99,98,..." style="width:100%;padding:9px;border:1px solid var(--line);margin-top:4px"><small id="rsi-hint" style="display:block;color:var(--muted);margin-top:5px">Enter at least 15 values for the default period of 14.</small></label><label><span class="label">Period</span><input id="rsi-period" type="number" min="1" value="14" style="width:80px;padding:9px;border:1px solid var(--line);margin-top:4px"></label><button id="rsi-sample" type="button" class="command-button" style="padding:9px 14px;border:1px solid var(--line);background:var(--panel);color:var(--ink);cursor:pointer">Use sample</button><button id="rsi-run" type="button" class="command-button" style="padding:9px 14px;border:1px solid var(--teal);background:var(--teal);color:#fff;cursor:pointer">Calculate RSI</button></div><div id="rsi-result" role="status" aria-live="polite" style="margin-top:14px;padding:12px;background:#eef4f1;border-left:3px solid var(--teal);font-family:Arial,sans-serif;color:var(--muted)">Ready. Add prices or try the sample.</div>';view.prepend(tool);const closes=tool.querySelector('#rsi-closes'),period=tool.querySelector('#rsi-period'),hint=tool.querySelector('#rsi-hint'),result=tool.querySelector('#rsi-result');const updateHint=()=>{const count=closes.value.split(',').map(x=>x.trim()).filter(Boolean).length,needed=Number(period.value||14)+1;hint.textContent=count<needed?'Add '+(needed-count)+' more price'+(needed-count===1?'':'s')+' for this period.':count+' prices ready to calculate.'};closes.oninput=updateHint;period.oninput=updateHint;tool.querySelector('#rsi-sample').onclick=()=>{closes.value='100,102,101,104,103,105,106,104,107,109,108,110,111,109,112';updateHint();closes.focus()};tool.querySelector('#rsi-run').onclick=async()=>{result.textContent='Calculating...';try{const response=await fetch('/api/rsi?closes='+encodeURIComponent(closes.value)+'&period='+encodeURIComponent(period.value)),data=await response.json();if(data.error)throw Error(data.error);const color=data.interpretation==='overbought'?'var(--orange)':data.interpretation==='oversold'?'var(--green)':'var(--teal)';result.style.borderLeftColor=color;result.innerHTML='<strong>RSI '+Number(data.value).toFixed(2)+'</strong> <span style="text-transform:capitalize">'+esc(data.interpretation)+'</span><br><small>'+esc(data.reason)+' This is a review signal, not a trade instruction.</small>'}catch(error){result.style.borderLeftColor='var(--red)';result.textContent='Check the prices and period: '+error.message}};updateHint()}
const fundamentalSummaryObserver=new MutationObserver(addFundamentalSummary);fundamentalSummaryObserver.observe(document.body,{childList:true,subtree:true});
function fundamentalSummary(data){const rows=[];(data.key_ratios||[]).forEach(item=>{const name=item.name||item.key||'Ratio',company=Number(item.company_value),sector=Number(item.sector_value);let meaning='Reported value for this company.';if(Number.isFinite(company)&&Number.isFinite(sector)){meaning=company>sector?'Higher than the sector average.':company<sector?'Lower than the sector average.':'In line with the sector average.'}rows.push('<tr><td>'+esc(name)+'</td><td>'+esc(fundamentalValue(item.company_value))+'</td><td>'+esc(meaning)+'</td></tr>')});const profile=data.profile||{};if(profile.sector)rows.unshift('<tr><td>Sector</td><td>'+esc(profile.sector)+'</td><td>This is the industry used for comparison.</td></tr>');if(!rows.length)rows.push('<tr><td colspan="3" class="fundamental-empty">No summary data returned.</td></tr>');return '<table class="fundamental-summary-table"><tr><th>Parameter</th><th>Value</th><th>In simple terms</th></tr>'+rows.join('')+'</table>'}
async function showFundamentalsPopup(symbol,target){const old=document.querySelector('.fundamental-popup');if(old)old.remove();const popup=document.createElement('div');popup.className='panel fundamental-popup';popup.innerHTML='<h2>'+esc(symbol)+'</h2><p class="loading">Loading fundamentals...</p>';document.body.appendChild(popup);const place=()=>{const box=target.getBoundingClientRect(),width=Math.min(760,window.innerWidth-24);popup.style.cssText='position:fixed;z-index:30;width:'+width+'px;max-height:calc(100vh - 24px);overflow:auto;left:'+Math.max(12,Math.min(box.left,window.innerWidth-width-12))+'px;top:'+Math.min(box.bottom+8,Math.max(12,window.innerHeight-popup.offsetHeight-12))+'px'};place();let data=window.__fundamentalCache?.[symbol];try{if(!data){data=await safeJsonFetch('/api/fundamentals?symbol='+encodeURIComponent(symbol),{status:'error',error:'Fundamentals are unavailable right now.'});window.__fundamentalCache=window.__fundamentalCache||{};window.__fundamentalCache[symbol]=data}if(!popup.isConnected)return;if(data.status==='running'){setTimeout(()=>{if(popup.isConnected)showFundamentalsPopup(symbol,target)},1000);return}if(data.error)throw Error(data.error);const actions=data.corporate_actions||[],ratios=data.key_ratios||[];popup.innerHTML='<h2>'+esc(data.symbol)+' <span class="label">'+esc(data.isin)+'</span></h2><div class="fundamental-sections"><section><h3>Profile</h3><div class="fundamental-fields">'+fundamentalRows(data.profile)+'</div></section><section><h3>Key ratios</h3><div class="fundamental-fields">'+(ratios.length?ratios.map(item=>'<div class="fundamental-ratio"><b>'+esc(item.name||item.key||'Ratio')+'</b><span>Company: '+esc(fundamentalValue(item.company_value))+'</span><span>Sector: '+esc(fundamentalValue(item.sector_value))+'</span></div>').join(''):'<span class="fundamental-empty">No data</span>')+'</div></section><section><h3>Balance sheet</h3>'+fundamentalHistory(data.balance_sheet)+'</section><section><h3>Income statement</h3>'+fundamentalHistory(data.income_statement)+'</section><section><h3>Cash flow</h3>'+fundamentalHistory(data.cash_flow)+'</section><section><h3>Corporate actions</h3>'+fundamentalHistory(actions)+'</section></div>';place()}catch(error){if(popup.isConnected)popup.innerHTML='<h2>'+esc(symbol)+'</h2><p class="error">Unable to load fundamentals: '+esc(error.message)+'</p>'}popup.onmouseenter=()=>popup.dataset.inside='1';popup.onmouseleave=()=>{popup.dataset.inside='';popup.remove()}}
document.addEventListener('click',event=>{const target=event.target.closest('.fundamental-stock,.fundamental-open');if(target)showFundamentalsPopup(target.dataset.symbol,target)});
document.addEventListener('mouseover',event=>{const target=event.target.closest('.fundamental-stock');if(!target||target.contains(event.relatedTarget)||document.querySelector('.fundamental-popup'))return;if(!document.querySelector('#fundamental-popup-style'))document.head.insertAdjacentHTML('beforeend','<style id="fundamental-popup-style">.fundamental-popup{padding:18px;font-family:Arial,sans-serif;max-width:min(760px,calc(100vw - 32px));max-height:min(72vh,720px);overflow:auto;white-space:normal}.fundamental-popup h2{font-family:Georgia,serif;margin-bottom:12px}.fundamental-sections{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:10px}.fundamental-sections section{border:1px solid var(--line);padding:10px;min-width:0}.fundamental-sections h3{font:700 11px Arial,sans-serif;letter-spacing:1px;text-transform:uppercase;color:var(--teal);margin:0 0 8px}.fundamental-fields{display:grid;gap:4px}.fundamental-field{display:flex;justify-content:space-between;gap:12px;border-bottom:1px solid #ded9cf88;padding:2px 0;font-size:11px}.fundamental-field span{color:var(--muted);text-transform:capitalize}.fundamental-field b{font-weight:600;text-align:right;overflow-wrap:anywhere}.fundamental-row{border-bottom:1px solid #ded9cf88;padding:3px 0}.fundamental-ratio{display:grid;grid-template-columns:1.2fr 1fr 1fr;gap:6px;font-size:11px;border-bottom:1px solid #ded9cf88;padding:3px 0}.fundamental-ratio span{color:var(--muted)}.fundamental-empty{color:var(--muted);font-size:11px}@media(max-width:650px){.fundamental-sections{grid-template-columns:1fr}.fundamental-ratio{grid-template-columns:1fr}}.fundamental-popup .fundamental-summary-table{width:100%;white-space:normal}.fundamental-popup .fundamental-summary-table th,.fundamental-popup .fundamental-summary-table td{vertical-align:top}</style>');showFundamentalsPopup(target.dataset.symbol,target)});
document.addEventListener('mouseout',event=>{const target=event.target.closest('.fundamental-stock'),popup=document.querySelector('.fundamental-popup');if(target&&!target.contains(event.relatedTarget)&&!popup?.contains(event.relatedTarget)){if(popup)popup.remove()}});
new MutationObserver(()=>{document.querySelectorAll('.fundamental-open').forEach(button=>button.parentElement.remove());const table=document.querySelector('.fundamental-stock')?.closest('table');if(table&&table.rows[0]?.lastElementChild?.textContent==='Open')table.rows[0].lastElementChild.remove()}).observe(view,{childList:true,subtree:true});
new MutationObserver(addWorkflowReference).observe(view,{childList:true});new MutationObserver(addRsiTool).observe(view,{childList:true});new MutationObserver(normalizeAlertsTable).observe(view,{childList:true,subtree:true});
setInterval(()=>{if(window.__fundamentalCache)Object.keys(window.__fundamentalCache).forEach(symbol=>{if(window.__fundamentalCache[symbol]?.status==='running')delete window.__fundamentalCache[symbol]})},900);
document.addEventListener('click',event=>{const target=event.target.closest('.holding-symbol');if(target)showStockAnalysis(target.textContent.trim())});new MutationObserver(()=>{addSerialNumbers();normalizeRefreshMetric();normalizeDeeperAnalysis()}).observe(view,{childList:true,subtree:true});document.querySelectorAll('.tab').forEach(x=>{x.onclick=()=>openTab(x.dataset.tab);x.onkeydown=event=>{if(!['ArrowRight','ArrowLeft'].includes(event.key))return;event.preventDefault();const tabs=[...document.querySelectorAll('.tab')],index=tabs.indexOf(x),next=tabs[(index+(event.key==='ArrowRight'?1:-1)+tabs.length)%tabs.length];next.focus();openTab(next.dataset.tab)}});document.querySelector('#view-filter').oninput=event=>filterView(event.target.value);document.querySelector('#refresh-view').onclick=refreshView;document.querySelector('#pause-refresh').onclick=toggleUpdates;updateMarketStatus();openTab('overview').then(sortOverviewRows);
setInterval(()=>{if(activeTab==='deeper-analysis'&&!updatesPaused){cache={};openTab(activeTab,false)}},30000);
refreshTimer=setInterval(()=>{updateMarketStatus();if(!updatesPaused&&!refreshInFlight&&activeTab!=='config'&&activeTab!=='deeper-analysis'){cache={};refreshInFlight=true;openTab(activeTab,false).finally(()=>refreshInFlight=false)}},15000);
</script></body></html>)HTML";
}

std::unordered_set<std::string> csvSymbols(const std::string& path) {
    std::unordered_set<std::string> result;
    std::ifstream input(path);
    std::string line;
    bool header = true;
    while (std::getline(input, line)) {
        if (header) { header = false; continue; }
        const auto comma = line.find(',');
        const auto symbol = normalizeSymbol(line.substr(0, comma));
        if (!symbol.empty()) result.insert(symbol);
    }
    return result;
}

json localHoldings(const std::string& path) {
    std::ifstream input(path);
    if (!input) return json();
    json rows = json::array();
    std::string line;
    bool header = true;
    while (std::getline(input, line)) {
        if (header) { header = false; continue; }
        std::stringstream fields(line);
        std::vector<std::string> values;
        std::string value;
        while (std::getline(fields, value, ',')) values.push_back(value);
        if (values.size() < 5 || values[0].empty()) continue;
        try {
            rows.push_back({
                {"trading_symbol", values[0]},
                {"tradingsymbol", values[0]},
                {"instrument_token", values[0]},
                {"exchange", values[1]},
                {"quantity", std::stol(values[2])},
                {"average_price", std::stod(values[3])},
                {"last_price", std::stod(values[4])},
                {"pnl", values.size() > 8 ? std::stod(values[8]) : 0.0},
                {"unrealised", values.size() > 8 ? std::stod(values[8]) : 0.0}});
        } catch (...) {
            continue;
        }
    }
    return json({{"status", "success"}, {"data", rows}, {"source", "local config/holding.csv"}});
}

std::string localNews(const json& localHoldings, const std::string& holdingsFile) {
    std::ifstream input("config/portfolio_news.json");
    if (!input) return json({{"status", "success"}, {"data", json::object()},
                             {"alerts", json::array()}, {"source", "local"}}).dump();
    std::ostringstream content;
    content << input.rdbuf();
    const json source = json::parse(content.str());
    json output = source;
    output["data"] = json::object();
    output["alerts"] = json::array();
    const auto allowedSymbols = csvSymbols(holdingsFile);
    std::unordered_set<std::string> allowedKeys;
    if (localHoldings.contains("data") && localHoldings["data"].is_array()) {
        for (const auto& holding : localHoldings["data"]) {
            const std::string symbol = normalizeSymbol(
                holding.value("trading_symbol", holding.value("tradingsymbol", "")));
            if (allowedSymbols.count(symbol)) {
                allowedKeys.insert(holding.value("instrument_token", ""));
            }
        }
    }
    if (source.contains("data") && source["data"].is_object()) {
        for (const auto& entry : source["data"].items()) {
            if (!allowedKeys.count(entry.key())) continue;
            json recent = json::array();
            const auto now = std::chrono::system_clock::now();
            for (const auto& article : entry.value()) {
                const long long published = article.value("published_time", 0LL);
                const auto articleTime = std::chrono::system_clock::time_point(
                    std::chrono::milliseconds(published));
                if (published > 0 && articleTime <= now &&
                    now - articleTime <= std::chrono::hours(24 * 30)) {
                    recent.push_back(article);
                }
            }
            if (!recent.empty()) output["data"][entry.key()] = std::move(recent);
        }
    }
    return output.dump(2);
}

struct Sentiment {
    double score = 0.0;
    std::string label = "neutral";
    std::string signal = "Hold / monitor";
};

struct NewsDecision {
    double score = 0.0;
    double confidence = 0.0;
    std::string label = "neutral";
    std::string action = "Hold / wait";
    std::string rationale = "News is mixed or inconclusive.";
};

Sentiment scoreNews(const json& item) {
    std::string text = item.value("heading", "") + " " +
                       item.value("summary", "");
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    const std::vector<std::string> positive = {
        "buy", "upgrade", "beat", "gain", "surge", "rally", "profit",
        "strong", "positive", "outperform", "order", "contract", "rise"};
    const std::vector<std::string> negative = {
        "sell", "downgrade", "miss", "loss", "drop", "decline", "fall",
        "fraud", "investigation", "weak", "negative", "underperform", "cut"};
    int positives = 0;
    int negatives = 0;
    for (const auto& word : positive) if (text.find(word) != std::string::npos) ++positives;
    for (const auto& word : negative) if (text.find(word) != std::string::npos) ++negatives;
    const int total = positives + negatives;
    Sentiment result;
    if (total > 0) result.score = static_cast<double>(positives - negatives) / total;
    if (result.score >= 0.30) {
        result.label = "positive";
        result.signal = "Positive signal - review";
    } else if (result.score <= -0.30) {
        result.label = "negative";
        result.signal = "Risk alert - review";
    }
    return result;
}

NewsDecision decideNews(const json& items) {
    NewsDecision result;
    if (!items.is_array() || items.empty()) return result;

    const auto now = std::chrono::system_clock::now();
    double weightedScore = 0.0;
    double totalWeight = 0.0;
    int directionalArticles = 0;
    for (const auto& article : items) {
        const Sentiment sentiment = scoreNews(article);
        double weight = 1.0;
        const long long published = article.value("published_time", 0LL);
        if (published > 0) {
            const auto publishedAt = std::chrono::system_clock::time_point(
                std::chrono::milliseconds(published));
            const double ageDays = std::max(0.0, std::chrono::duration<double>(
                now - publishedAt).count() / 86400.0);
            weight = 1.0 / (1.0 + ageDays);
        }
        weightedScore += sentiment.score * weight;
        totalWeight += weight;
        if (sentiment.score != 0.0) ++directionalArticles;
    }
    if (totalWeight == 0.0) return result;

    result.score = weightedScore / totalWeight;
    const double coverage = static_cast<double>(directionalArticles) / items.size();
    const double agreement = std::abs(result.score);
    result.confidence = std::min(1.0, coverage * 0.5 + agreement * 0.5);
    if (result.score >= 0.30 && result.confidence >= 0.35) {
        result.label = "positive";
        result.action = "Consider adding - review";
        result.rationale = "Recent holding news is mostly positive.";
    } else if (result.score <= -0.30 && result.confidence >= 0.35) {
        result.label = "negative";
        result.action = "Do not add - review risk";
        result.rationale = "Recent holding news is mostly negative.";
    } else {
        result.rationale = directionalArticles == 0
            ? "No clear sentiment keywords were found."
            : "News sentiment is mixed or confidence is too low.";
    }
    return result;
}

std::string filteredNews(const std::string& body,
                         const std::string& holdingsFile,
                         const std::vector<Position>& holdings) {
    try {
        if (body.empty()) return json({"data", json::object(), "alerts", json::array()}).dump();
        const auto symbols = csvSymbols(holdingsFile);
        const json source = json::parse(body);
        json output = source;
        output["data"] = json::object();
        output["alerts"] = json::array();
        std::unordered_set<std::string> keys;
        for (const auto& holding : holdings) {
            if (symbols.count(normalizeSymbol(holding.tradingSymbol))) {
                keys.insert(holding.instrumentToken);
            }
        }
        if (source.contains("data") && source["data"].is_object()) {
            for (auto it = source["data"].begin(); it != source["data"].end(); ++it)
                    if (keys.count(it.key())) {
                        json items = json::array();
                        for (const auto& article : it.value()) {
                            const long long published = article.value("published_time", 0LL);
                            const auto now = std::chrono::system_clock::now();
                            const auto articleTime = std::chrono::system_clock::time_point(
                                std::chrono::milliseconds(published));
                            if (published <= 0 || articleTime > now ||
                                now - articleTime > std::chrono::hours(24 * 30)) continue;
                            json enriched = article;
                            const Sentiment sentiment = scoreNews(article);
                            enriched["sentiment_score"] = sentiment.score;
                            enriched["sentiment"] = sentiment.label;
                            enriched["signal"] = sentiment.signal;
                            items.push_back(std::move(enriched));
                        }
                        const NewsDecision decision = decideNews(items);
                        output["data"][it.key()] = std::move(items);
                        output["alerts"].push_back({
                            {"instrument_key", it.key()},
                            {"sentiment_score", decision.score},
                            {"confidence", decision.confidence},
                            {"sentiment", decision.label},
                            {"action", decision.action},
                            {"rationale", decision.rationale},
                            {"article_count", static_cast<int>(output["data"][it.key()].size())}});
                    }
        }
        return output.dump(2);
    } catch (const std::exception& e) {
        return json({"data", json::object(), "alerts", json::array(), "error", std::string(e.what())}).dump();
    }
}

std::string metrics(const std::vector<Position>& holdings, int newsArticles,
                    const json& news) {
    std::ostringstream out;
    double marketValue = 0.0;
    double totalPnl = 0.0;
    int considerCount = 0;
    int riskCount = 0;
    int holdCount = 0;
    for (const auto& holding : holdings) {
        marketValue += holding.marketValue();
        totalPnl += holding.unrealised + holding.realised;
    }
    std::unordered_map<std::string, std::string> actions;
    if (news.contains("alerts") && news["alerts"].is_array()) {
        for (const auto& alert : news["alerts"]) {
            actions[alert.value("instrument_key", "")] = alert.value("action", "");
        }
    }
    for (const auto& holding : holdings) {
        const auto action = actions.find(holding.instrumentToken);
        if (action == actions.end() || action->second.empty()) ++holdCount;
        else if (action->second.rfind("Consider", 0) == 0) ++considerCount;
        else if (action->second.rfind("Do not", 0) == 0) ++riskCount;
        else ++holdCount;
    }
    out << "# HELP portfolio_holdings_total Number of long-term holdings.\n"
        << "# TYPE portfolio_holdings_total gauge\n"
        << "portfolio_holdings_total " << holdings.size() << "\n"
        << "# HELP portfolio_market_value Current market value of holdings.\n"
        << "# TYPE portfolio_market_value gauge\n"
        << "portfolio_market_value " << marketValue << "\n"
        << "# HELP portfolio_total_pnl Total unrealised and realised P&L.\n"
        << "# TYPE portfolio_total_pnl gauge\n"
        << "portfolio_total_pnl " << totalPnl << "\n"
        << "# HELP portfolio_news_articles_total News articles matching holding.csv.\n"
        << "# TYPE portfolio_news_articles_total gauge\n"
        << "portfolio_news_articles_total " << newsArticles << "\n"
        << "# HELP portfolio_news_review_total Holdings with each news review action.\n"
        << "# TYPE portfolio_news_review_total gauge\n"
        << "portfolio_news_review_total{action=\"consider\"} " << considerCount << "\n"
        << "portfolio_news_review_total{action=\"risk\"} " << riskCount << "\n"
        << "portfolio_news_review_total{action=\"hold\"} " << holdCount << "\n";
    return out.str();
}

std::string holdingsForUi(const std::string& body,
                          const std::vector<Position>& holdings) {
    try {
        json payload = json::parse(body);
        auto data = payload.find("data");
        if (data == payload.end() || !data->is_array()) {
            if (body.empty() || body == "{}") {
                return json({"status", "success", "data", json::array(), "source", "fallback"}).dump();
            }
            return body;
        }
        std::unordered_map<std::string, double> values;
        double total = 0.0;
        for (const auto& holding : holdings) {
            values[holding.instrumentToken] = holding.marketValue();
            total += holding.marketValue();
        }
        payload["portfolio_market_value"] = total;
        payload["source"] = "upstox-live";
        for (auto& item : *data) {
            const std::string key = item.value("instrument_token", "");
            const auto value = values.find(key);
            if (value != values.end()) item["current_value"] = value->second;
        }
        return payload.dump(2);
    } catch (const std::exception& e) {
        return json({"status", "error", "error", std::string(e.what()), "data", json::array()}).dump();
    }
}

std::string ensureValidJson(const std::string& body) {
    if (body.empty()) return json({}).dump();
    try {
        json::parse(body);
        return body;
    } catch (const std::exception& e) {
        return json({"error", "Invalid JSON response", "details", std::string(e.what())}).dump();
    }
}

std::string response(const std::string& body, const std::string& type = "application/json") {
    const std::string safeBody = type == "application/json" ? ensureValidJson(body) : body;
    return "HTTP/1.1 200 OK\r\nContent-Type: " + type +
           "; charset=utf-8\r\nCache-Control: no-store, no-cache, must-revalidate\r\nPragma: no-cache\r\nContent-Length: " + std::to_string(safeBody.size()) +
           "\r\nConnection: close\r\n\r\n" + safeBody;
}

std::string responseWithStatus(const std::string& status,
                               const std::string& body,
                               const std::string& type = "application/json",
                               const std::string& extraHeaders = {}) {
    return "HTTP/1.1 " + status + "\r\nContent-Type: " + type +
           "; charset=utf-8\r\nCache-Control: no-store, no-cache, must-revalidate\r\nPragma: no-cache\r\nContent-Length: " + std::to_string(body.size()) +
           "\r\nConnection: close\r\n" + extraHeaders + "\r\n" + body;
}

std::string cookieValue(const std::string& request) {
    const auto start = request.find("Cookie:");
    if (start == std::string::npos) return {};
    const auto end = request.find("\r\n", start);
    const std::string cookies = request.substr(start, end - start);
    const std::string prefix = "session=";
    const auto valueStart = cookies.find(prefix);
    if (valueStart == std::string::npos) return {};
    const auto first = valueStart + prefix.size();
    const auto last = cookies.find(';', first);
    return cookies.substr(first, last == std::string::npos ? std::string::npos : last - first);
}

std::string urlDecode(const std::string& value);

std::string trimWhitespace(const std::string& value) {
    const auto start = value.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return {};
    const auto end = value.find_last_not_of(" \t\r\n");
    return value.substr(start, end - start + 1);
}

bool secureEquals(const std::string& lhs, const std::string& rhs) {
    if (lhs.size() != rhs.size()) return false;
    volatile unsigned char diff = 0;
    for (std::size_t i = 0; i < lhs.size(); ++i) {
        diff |= static_cast<unsigned char>(lhs[i]) ^ static_cast<unsigned char>(rhs[i]);
    }
    return diff == 0;
}

std::string formValue(const std::string& body, const std::string& name) {
    const std::string prefix = name + "=";
    const auto valueStart = body.find(prefix);
    if (valueStart == std::string::npos) return {};
    const auto first = valueStart + prefix.size();
    const auto last = body.find('&', first);
    const std::string encoded = body.substr(first, last == std::string::npos ? std::string::npos : last - first);
    return normalizeLoginCode(encoded);
}

std::string configuredLoginCode() {
    const char* configured = std::getenv("FOLIO_LOGIN_CODE");
    if (configured != nullptr) {
        const std::string value = normalizeLoginCode(configured);
        if (!value.empty()) return value;
    }

    std::vector<std::string> candidates = {".folio_login_code"};
    const char* home = std::getenv("HOME");
    if (home != nullptr) candidates.emplace_back(std::string(home) + "/.folio_login_code");

    for (const auto& path : candidates) {
        std::ifstream codeFile(path);
        if (!codeFile.is_open()) continue;
        std::string value;
        std::getline(codeFile, value);
        const std::string trimmed = normalizeLoginCode(value);
        if (!trimmed.empty()) return trimmed;
    }
    return {};
}

std::string urlDecode(const std::string& value) {
    std::string decoded;
    decoded.reserve(value.size());
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (value[index] == '%' && index + 2 < value.size() &&
            std::isxdigit(static_cast<unsigned char>(value[index + 1])) &&
            std::isxdigit(static_cast<unsigned char>(value[index + 2]))) {
            const auto digit = [](char character) {
                if (character >= '0' && character <= '9') return character - '0';
                if (character >= 'a' && character <= 'f') return character - 'a' + 10;
                return character - 'A' + 10;
            };
            decoded.push_back(static_cast<char>(digit(value[index + 1]) * 16 + digit(value[index + 2])));
            index += 2;
        } else {
            decoded.push_back(value[index] == '+' ? ' ' : value[index]);
        }
    }
    return decoded;
}

bool sendAll(int socket, const std::string& payload) {
    std::size_t sent = 0;
    while (sent < payload.size()) {
        const ssize_t count = send(socket, payload.data() + sent,
                                   payload.size() - sent, MSG_NOSIGNAL);
        if (count > 0) {
            sent += static_cast<std::size_t>(count);
            continue;
        }
        if (count < 0 && errno == EINTR) continue;
        return false;
    }
    return true;
}

std::string queryValue(const std::string& path, const std::string& name) {
    const std::string prefix = name + "=";
    const auto queryStart = path.find('?');
    if (queryStart == std::string::npos) return {};
    const auto valueStart = path.find(prefix, queryStart + 1);
    if (valueStart == std::string::npos) return {};
    const auto first = valueStart + prefix.size();
    const auto last = path.find('&', first);
    return urlDecode(path.substr(first, last == std::string::npos ? std::string::npos : last - first));
}

std::vector<std::string> instrumentKeys(const std::string& encodedKeys) {
    std::vector<std::string> keys;
    std::stringstream input(encodedKeys);
    std::string key;
    while (std::getline(input, key, ',')) if (!key.empty()) keys.push_back(key);
    return keys;
}

std::vector<double> closePrices(const std::string& encodedCloses) {
    std::vector<double> closes;
    std::stringstream input(encodedCloses);
    std::string value;
    while (std::getline(input, value, ',')) {
        if (value.empty()) continue;
        try { closes.push_back(std::stod(value)); }
        catch (...) { return {}; }
    }
    return closes;
}

} // namespace

WebServer::WebServer(const UpstoxClient& client, std::string holdingsFile, int port)
    : client_(client), holdingsFile_(std::move(holdingsFile)), port_(port) {}

std::string WebServer::fundamentalsAnalysis(const std::string& symbol) const {
    const auto holdings = client_.getHoldings();
    if (!holdings.ok) throw std::runtime_error(holdings.error);
    for (const auto& holding : holdings.positions) {
        if (holding.tradingSymbol != symbol) continue;
        const auto separator = holding.instrumentToken.find('|');
        const std::string isin = separator == std::string::npos
            ? holding.instrumentToken : holding.instrumentToken.substr(separator + 1);
        const auto data = client_.getFundamentals(isin);
        if (!data.ok) throw std::runtime_error(data.error);
        return json({
            {"symbol", symbol},
            {"isin", isin},
            {"profile", json::parse(data.profileBody).value("data", json::object())},
            {"balance_sheet", json::parse(data.balanceSheetBody).value("data", json::object())},
            {"income_statement", json::parse(data.incomeStatementBody).value("data", json::object())},
            {"cash_flow", json::parse(data.cashFlowBody).value("data", json::object())},
            {"corporate_actions", json::parse(data.corporateActionsBody).value("data", json::array())},
            {"key_ratios", json::parse(data.ratiosBody).value("data", json::array())}}).dump(2);
    }
    throw std::runtime_error("stock not found in live holdings");
}

std::filesystem::path projectRoot() {
    std::vector<std::filesystem::path> roots = {
        std::filesystem::current_path(),
        std::filesystem::absolute("."),
        std::filesystem::weakly_canonical(std::filesystem::path("."))
    };
    const auto procExe = std::filesystem::exists("/proc/self/exe")
        ? std::filesystem::canonical("/proc/self/exe")
        : std::filesystem::path();
    if (!procExe.empty()) {
        const auto procDir = procExe.parent_path();
        if (!procDir.empty()) roots.push_back(procDir);
    }
    for (const auto& root : roots) {
        const auto candidate = root / "config" / "holding.csv";
        if (std::filesystem::exists(candidate)) return root;
        const auto parentCandidate = root.parent_path() / "config" / "holding.csv";
        if (std::filesystem::exists(parentCandidate)) return root.parent_path();
    }
    return std::filesystem::current_path();
}

std::string projectPythonScript() {
    const auto root = projectRoot();
    const std::vector<std::filesystem::path> candidates = {
        root / "stock_alert_nlp.py",
        root.parent_path() / "stock_alert_nlp.py",
        std::filesystem::current_path() / "stock_alert_nlp.py",
        std::filesystem::absolute("stock_alert_nlp.py")
    };
    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate)) return candidate.string();
    }
    return (root / "stock_alert_nlp.py").string();
}

std::string WebServer::stockAnalysis(const std::string& symbol) const {
    if (symbol.empty() || symbol.size() > 32 ||
        symbol.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789&.-") != std::string::npos) {
        throw std::runtime_error("invalid stock symbol");
    }
    const std::string scriptPath = projectPythonScript();
    std::ifstream savedFile("config/portfolio_news.json");
    if (!savedFile) throw std::runtime_error("config/portfolio_news.json not found");
    std::ostringstream savedText;
    savedText << savedFile.rdbuf();
    const json saved = json::parse(savedText.str());
    const auto holdings = client_.getHoldings();
    if (!holdings.ok) throw std::runtime_error(holdings.error);

    std::string instrumentKey;
    for (const auto& holding : holdings.positions) {
        if (holding.tradingSymbol == symbol) {
            instrumentKey = holding.instrumentToken;
            break;
        }
    }
    if (instrumentKey.empty()) throw std::runtime_error("stock not found in live holdings");
    const std::size_t separator = instrumentKey.find('|');
    const std::string isin = separator == std::string::npos
        ? instrumentKey : instrumentKey.substr(separator + 1);
    const auto fundamentals = client_.getFundamentals(isin);

    const auto root = projectRoot();
    const std::string inputPath = "/tmp/baran_capital_view_stock_" +
                                  std::to_string(static_cast<long long>(getpid())) + ".csv";
    const std::string outputPath = "/tmp/baran_capital_view_stock_" +
                                   std::to_string(static_cast<long long>(getpid())) + ".json";
    const auto holdingsFile = (root / "config" / "holding.csv").string();
    {
        std::ofstream input(inputPath);
        if (!input) throw std::runtime_error("cannot create Python input file");
        input << "symbol,name\n" << symbol << ",\n";
    }
    const std::string command = "python3 \"" + scriptPath + "\" \"" + inputPath + "\" \"" +
        outputPath + "\" --json > /dev/null 2>&1";
    const int exitCode = std::system(command.c_str());
    std::remove(inputPath.c_str());
    if (exitCode != 0) {
        std::remove(outputPath.c_str());
        throw std::runtime_error("stock_alert_nlp.py failed; check config/news_config.json");
    }
    std::ifstream resultFile(outputPath);
    std::ostringstream resultText;
    resultText << resultFile.rdbuf();
    std::remove(outputPath.c_str());
    const json pythonResults = json::parse(resultText.str());
    const json python = pythonResults.empty() ? json::object() : pythonResults.front();

    json upstoxArticles = json::array();
    if (saved.contains("data") && saved["data"].is_object() &&
        saved["data"].contains(instrumentKey) && saved["data"][instrumentKey].is_array()) {
        upstoxArticles = saved["data"][instrumentKey];
    }
    double savedScore = 0.0;
    if (saved.contains("alerts") && saved["alerts"].is_array()) {
        for (const auto& alert : saved["alerts"]) {
            if (alert.value("instrument_key", "") == instrumentKey) {
                savedScore = alert.value("sentiment_score", 0.0);
                break;
            }
        }
    }
    const std::string recommendation = python.value("recommendation", "No recent news");
    const bool savedPositive = savedScore >= 0.30;
    const bool savedNegative = savedScore <= -0.30;
    const bool freshPositive = recommendation == "invest more" || recommendation == "going good";
    const bool freshNegative = recommendation == "sell it off";
    std::string fundamentalsSignal = "Unavailable";
    std::string fundamentalsSummary = fundamentals.error;
    json ratios = json::array();
    json profile = json::object();
    if (fundamentals.ok) {
        const json profileResponse = json::parse(fundamentals.profileBody);
        const json ratiosResponse = json::parse(fundamentals.ratiosBody);
        profile = profileResponse.value("data", json::object());
        ratios = ratiosResponse.value("data", json::array());
        int better = 0;
        int worse = 0;
        for (const auto& ratio : ratios) {
            double company = 0.0;
            double sector = 0.0;
            try {
                company = std::stod(ratio.value("company_value", "0"));
                sector = std::stod(ratio.value("sector_value", "0"));
            } catch (...) {
                continue;
            }
            const std::string name = ratio.value("name", "");
            const bool lowerIsBetter = name == "P/E" || name == "P/B" || name == "EV/EBITDA";
            if (company == sector) continue;
            if ((lowerIsBetter && company < sector) || (!lowerIsBetter && company > sector)) ++better;
            else ++worse;
        }
        fundamentalsSignal = better > worse ? "Favorable" : worse > better ? "Caution" : "Mixed";
        fundamentalsSummary = std::to_string(better) + " ratios favorable versus sector, " +
                              std::to_string(worse) + " less favorable.";
    }
    std::string action = "Hold / review";
    std::string comparison = "Both sources are neutral or inconclusive.";
    const bool fundamentalsPositive = fundamentalsSignal == "Favorable";
    const bool fundamentalsNegative = fundamentalsSignal == "Caution";
    if (savedPositive && freshPositive && fundamentalsPositive) {
        action = "Buy / review";
        comparison = "News agrees and fundamentals are favorable versus the sector.";
    } else if (savedNegative && freshNegative && fundamentalsNegative) {
        action = "Sell / review";
        comparison = "News is negative and fundamentals are weaker versus the sector.";
    } else if (savedPositive || savedNegative || freshPositive || freshNegative ||
               fundamentalsPositive || fundamentalsNegative) {
        comparison = "News and fundamentals are not aligned; review before acting.";
    }
    return json({
        {"symbol", symbol},
        {"action", action},
        {"comparison", comparison},
        {"fundamentals_signal", fundamentalsSignal},
        {"fundamentals_summary", fundamentalsSummary},
        {"company_profile", profile},
        {"key_ratios", ratios},
        {"upstox_signal", savedPositive ? "Positive" : savedNegative ? "Negative" : "Neutral"},
        {"news_config_recommendation", recommendation},
        {"upstox_news", upstoxArticles},
        {"news_config_news", python.value("articles", json::array())}}).dump(2);
}

std::string WebServer::runDeeperAnalysis() const {
    std::ifstream savedFile("config/portfolio_news.json");
    if (!savedFile) throw std::runtime_error("config/portfolio_news.json not found");
    std::ostringstream savedText;
    savedText << savedFile.rdbuf();
    const json saved = json::parse(savedText.str());

    const auto holdings = client_.getHoldings();
    if (!holdings.ok) throw std::runtime_error(holdings.error);

    const std::string outputPath = "/tmp/baran_capital_view_nlp_" +
                                   std::to_string(static_cast<long long>(getpid())) + ".csv";
    const std::string scriptPath = projectPythonScript();
    const auto root = projectRoot();
    const auto holdingsFile = (root / "config" / "holding.csv").string();
    const std::string command = "python3 \"" + scriptPath + "\" \"" + holdingsFile + "\" \"" + outputPath + "\" --fast";
    const int exitCode = std::system(command.c_str());
    if (exitCode != 0) {
        std::remove(outputPath.c_str());
        throw std::runtime_error("stock_alert_nlp.py failed; check config/news_config.json");
    }

    std::unordered_map<std::string, std::string> recommendations;
    std::ifstream resultFile(outputPath);
    std::string line;
    if (std::getline(resultFile, line)) {
        while (std::getline(resultFile, line)) {
            std::stringstream row(line);
            std::string symbol;
            std::string name;
            std::string recommendation;
            if (std::getline(row, symbol, ',') &&
                std::getline(row, name, ',') &&
                std::getline(row, recommendation)) {
                recommendations[normalizeSymbol(symbol)] = recommendation;
            }
        }
    }
    std::remove(outputPath.c_str());

    std::unordered_map<std::string, std::string> symbols;
    for (const auto& holding : holdings.positions) {
        symbols[holding.instrumentToken] = holding.tradingSymbol;
    }

    json result = json::array();
    const auto alertIt = saved.find("alerts");
    std::unordered_map<std::string, json> savedAlerts;
    if (alertIt != saved.end() && alertIt->is_array()) {
        for (const auto& alert : *alertIt) {
            savedAlerts[alert.value("instrument_key", "")] = alert;
        }
    }
    const json savedData = saved.contains("data") && saved["data"].is_object()
        ? saved["data"] : json::object();
    json recentSavedData = json::object();
    const auto now = std::chrono::system_clock::now();
    for (const auto& entry : savedData.items()) {
        if (!symbols.count(entry.key()) || !entry.value().is_array()) continue;
        for (const auto& article : entry.value()) {
            const long long published = article.value("published_time", 0LL);
            const auto articleTime = std::chrono::system_clock::time_point(
                std::chrono::milliseconds(published));
            if (published > 0 && articleTime <= now &&
                now - articleTime <= std::chrono::hours(24 * 30)) {
                recentSavedData[entry.key()].push_back(article);
            }
        }
    }
    json categoryStocks = json::object();
    for (const auto& label : deeperAnalysisCategoryOrder()) {
        categoryStocks[label] = json::array();
    }
    for (const auto& holding : holdings.positions) {
        const std::string symbol = holding.tradingSymbol;
        const std::string key = holding.instrumentToken;
        const auto savedEntry = recentSavedData.find(key);
        const bool hasSavedNews = savedEntry != recentSavedData.end() && savedEntry->is_array() && !savedEntry->empty();
        const auto savedAlert = savedAlerts.find(key);
        const double score = savedEntry != recentSavedData.end()
            ? decideNews(*savedEntry).score
            : savedAlert == savedAlerts.end()
                ? 0.0 : savedAlert->second.value("sentiment_score", 0.0);
        const bool savedPositive = score >= 0.30;
        const bool savedNegative = score <= -0.30;
        const std::string normalizedSymbol = normalizeSymbol(symbol);
        const bool hasFreshRecommendation = recommendations.count(normalizedSymbol) != 0 &&
                                            recommendations[normalizedSymbol] != "No recent news";
        const std::string python = hasFreshRecommendation
            ? recommendations[normalizedSymbol]
            : hasSavedNews ? (savedPositive ? "Saved news: positive" : savedNegative ? "Saved news: negative" : "Saved news: neutral")
                           : "No recent news";
        const bool pythonPositive = hasFreshRecommendation &&
                                    (python == "invest more" || python == "going good");
        const bool pythonNegative = hasFreshRecommendation && python == "sell it off";
        const std::string category = hasFreshRecommendation ? python
            : hasSavedNews ? "Neutral news" : "No recent news";
        if (categoryStocks.contains(category)) categoryStocks[category].push_back(symbol);
        std::string action = "Hold / wait";
        std::string analysis = "Signals are mixed or neutral.";
        if (savedPositive && pythonPositive) {
            action = "Buy / review";
            analysis = "Saved portfolio news and fresh Python news are positive.";
        } else if (savedNegative && pythonNegative) {
            action = "Sell / review";
            analysis = "Saved portfolio news and fresh Python news are negative.";
        } else if (savedPositive || savedNegative || pythonPositive || pythonNegative) {
            analysis = hasFreshRecommendation
                ? "Saved and fresh news disagree; investigate before acting."
                : "Fresh Python news is unavailable; review the saved portfolio news.";
        }
        if (!hasFreshRecommendation && hasSavedNews) {
            analysis = "Fresh Python news is unavailable; saved portfolio news is available for review.";
        }
        result.push_back({
            {"symbol", symbol},
            {"saved_signal", savedPositive ? "Positive" : savedNegative ? "Negative" : "Neutral"},
            {"python_recommendation", python},
            {"python_source", hasFreshRecommendation ? "fresh Python news" : hasSavedNews ? "saved portfolio news fallback" : "no news source"},
            {"analysis", analysis},
            {"action", normalizeDecisionAction(action)},
            {"market_value", holding.marketValue()},
            {"article_count", hasSavedNews ? savedEntry->size() : 0}});
    }
    json categoryCounts = json::object();
    for (const auto& entry : categoryStocks.items())
        categoryCounts[entry.key()] = entry.value().size();
    return json({{"stocks", result}, {"category_counts", categoryCounts},
                 {"category_stocks", categoryStocks},
                 {"category_order", deeperAnalysisCategoryOrder()},
                 {"source", "config/portfolio_news.json + stock_alert_nlp.py + Upstox holdings"}}).dump(2);
}

bool WebServer::authenticated(const std::string& request) const {
    const std::string session = cookieValue(request);
    if (session.empty()) return false;
    std::lock_guard<std::mutex> lock(sessionMutex_);
    return sessions_.count(session) != 0;
}

std::string WebServer::newSession() {
    std::random_device device;
    std::mt19937_64 generator(device());
    std::uniform_int_distribution<unsigned long long> distribution;
    const std::string session = std::to_string(distribution(generator)) +
                                std::to_string(distribution(generator));
    std::lock_guard<std::mutex> lock(sessionMutex_);
    sessions_.insert(session);
    return session;
}

void WebServer::removeSession(const std::string& request) {
    const std::string session = cookieValue(request);
    if (session.empty()) return;
    std::lock_guard<std::mutex> lock(sessionMutex_);
    sessions_.erase(session);
}

std::shared_ptr<const WebServer::Snapshot> WebServer::snapshot() const {
    const auto now = std::chrono::steady_clock::now();
    {
        MutexGuard lock(snapshotMutex_);
        if (snapshot_ && now - snapshot_->created < std::chrono::seconds(5)) {
            return snapshot_;
        }
    }

    MutexGuard refreshLock(refreshMutex_);
    {
        MutexGuard lock(snapshotMutex_);
        if (snapshot_ && now - snapshot_->created < std::chrono::seconds(5)) {
            return snapshot_;
        }
    }

    const auto holdings = client_.getHoldings();
    if (!holdings.ok) {
        const json fallback = localHoldings(holdingsFile_);
        if (!fallback.is_object() || !fallback.contains("data") || fallback["data"].empty())
            throw std::runtime_error(holdings.error + ". Also no local portfolio data was found.");
        auto offline = std::make_shared<Snapshot>();
        offline->holdings = fallback.dump();
        json offlineHoldings = fallback;
        offlineHoldings["source"] = "local-fallback";
        offline->holdings = offlineHoldings.dump();
        offline->positions = json({{"status", "success"}, {"data", json::array()}, {"source", "offline"}}).dump();
        std::ifstream savedFile("config/portfolio_news.json");
        if (savedFile) {
            offline->news = localNews(fallback, holdingsFile_);
        } else {
            offline->news = localNews(fallback, holdingsFile_);
        }
        offline->metrics = metrics({}, 0, json::object());
        offline->created = now;
        std::lock_guard<std::mutex> lock(snapshotMutex_);
        snapshot_ = offline;
        return offline;
    }
    const auto positions = client_.getPositions();
    const auto news = client_.getNews("holdings");
    std::string filtered;
    if (news.ok) {
        filtered = filteredNews(news.rawBody, holdingsFile_, holdings.positions);
    } else {
        std::ifstream savedFile("config/portfolio_news.json");
        if (!savedFile) throw std::runtime_error(news.error);
        std::ostringstream savedBody;
        savedBody << savedFile.rdbuf();
        filtered = filteredNews(savedBody.str(), holdingsFile_, holdings.positions);
    }
    const auto filteredJson = json::parse(filtered);
    int articleCount = 0;
    if (filteredJson.contains("data") && filteredJson["data"].is_object()) {
        for (const auto& entry : filteredJson["data"].items()) {
            articleCount += static_cast<int>(entry.value().size());
        }
    }

    auto fresh = std::make_shared<Snapshot>();
    fresh->holdings = holdingsForUi(holdings.rawBody, holdings.positions);
    fresh->positions = positions.ok
        ? positions.rawBody
        : json({{"status", "error"}, {"data", json::array()},
                {"error", positions.error}}).dump();
    fresh->news = filtered;
    fresh->metrics = metrics(holdings.positions, articleCount, filteredJson);
    fresh->created = now;

    {
        MutexGuard lock(snapshotMutex_);
        if (!snapshot_ || snapshot_->created < fresh->created) snapshot_ = fresh;
        return snapshot_;
    }
}

int WebServer::run() {
    const int server = socket(AF_INET, SOCK_STREAM, 0);
    if (server < 0) { std::cerr << "Cannot create web server socket: " << std::strerror(errno) << "\n"; return 1; }
    int reuse = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(static_cast<uint16_t>(port_));
    if (bind(server, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0 || listen(server, 8) < 0) {
        std::cerr << "Cannot start web server on 127.0.0.1:" << port_ << ": " << std::strerror(errno) << "\n";
        close(server); return 1;
    }
    std::cout << "Portfolio web UI v" << PORTFOLIO_HEALTH_VERSION
              << ": http://127.0.0.1:" << port_
              << "\nPress Ctrl+C to stop.\n";
    while (true) {
        const int connection = accept(server, nullptr, nullptr);
        if (connection < 0) continue;
        const timeval socketTimeout{5, 0};
        setsockopt(connection, SOL_SOCKET, SO_RCVTIMEO,
               &socketTimeout, sizeof(socketTimeout));
        setsockopt(connection, SOL_SOCKET, SO_SNDTIMEO,
               &socketTimeout, sizeof(socketTimeout));
        char buffer[4096]{};
        const ssize_t bytes = read(connection, buffer, sizeof(buffer) - 1);
        if (bytes <= 0) { close(connection); continue; }
        std::istringstream request(std::string(buffer, static_cast<std::size_t>(bytes)));
        std::string method, path, version;
        request >> method >> path >> version;
        const std::string requestText(buffer, static_cast<std::size_t>(bytes));
        const auto bodyStart = requestText.find("\r\n\r\n");
        const std::string requestBody = bodyStart == std::string::npos
            ? std::string{} : requestText.substr(bodyStart + 4);
        std::string body;
        try {
            if (path == "/login") {
                const std::string html = loginPage();
                const std::string output = responseWithStatus(
                    "200 OK", html, "text/html");
                sendAll(connection, output);
                close(connection); continue;
            } else if (path == "/api/login" && method == "POST") {
                const std::string configuredCode = configuredLoginCode();
                const std::string submittedCode = formValue(requestBody, "code");
                if (!configuredCode.empty() && validateLoginCode(submittedCode, configuredCode)) {
                    const std::string cookie = "Set-Cookie: session=" +
                        newSession() + "; Max-Age=3600; HttpOnly; SameSite=Strict; Path=/\r\n";
                    const std::string output = responseWithStatus(
                        "303 See Other", {}, "text/plain",
                        cookie + "Location: /\r\n");
                    sendAll(connection, output);
                } else {
                    const std::string output = responseWithStatus(
                        "401 Unauthorized", "{\"error\":\"invalid code\"}");
                    sendAll(connection, output);
                }
                close(connection); continue;
            } else if (path == "/api/logout" && method == "POST") {
                removeSession(requestText);
                const std::string cookie =
                    "Set-Cookie: session=; Max-Age=0; HttpOnly; SameSite=Strict; Path=/\r\n";
                const std::string output = responseWithStatus(
                    "303 See Other", {}, "application/json",
                    cookie + "Location: /login\r\n");
                sendAll(connection, output);
                close(connection); continue;
            } else if (path == "/logout" && method == "GET") {
                removeSession(requestText);
                const std::string cookie =
                    "Set-Cookie: session=; Max-Age=0; HttpOnly; SameSite=Strict; Path=/\r\n";
                const std::string output = responseWithStatus(
                    "303 See Other", {}, "text/plain",
                    cookie + "Location: /login\r\n");
                sendAll(connection, output);
                close(connection); continue;
            } else if (!authenticated(requestText)) {
                const std::string output = responseWithStatus(
                    "303 See Other", {}, "text/plain", "Location: /login\r\n");
                sendAll(connection, output);
                close(connection); continue;
            } else if (method != "GET") body = "{\"error\":\"GET required\"}";
            else if (path == "/") {
                std::string html = page();
                const std::string marker = "</body>";
                html.replace(html.find(marker), marker.size(), std::string(dashboardVersion()) + releaseNotice() + sortingReleaseNotice() + categoryEnhancements() + marker);
                const std::string head = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nCache-Control: no-store\r\nContent-Length: " + std::to_string(html.size()) + "\r\nConnection: close\r\n\r\n";
                sendAll(connection, head + html);
                close(connection); continue;
            } else if (path.rfind("/api/market-quotes?instrument_key=", 0) == 0) {
                const auto keys = instrumentKeys(queryValue(path, "instrument_key"));
                if (keys.empty() || keys.size() > 500) {
                    body = json({{"error", "instrument_key must contain 1 to 500 instruments"}}).dump();
                } else {
                    const auto quotes = client_.fetchMarketQuotes(keys);
                    body = quotes.ok ? quotes.rawBody : json({{"error", quotes.error}}).dump();
                }
            } else if (path == "/api/market-feed/authorize") {
                const auto authorization = client_.authorizeMarketDataFeed();
                body = authorization.ok
                    ? json({{"status", "success"}, {"authorized_redirect_uri", authorization.authorizedRedirectUri}}).dump()
                    : json({{"error", authorization.error}}).dump();
            } else if (path.rfind("/api/rsi?closes=", 0) == 0) {
                const auto closes = closePrices(queryValue(path, "closes"));
                const auto periodText = queryValue(path, "period");
                std::size_t period = 14;
                if (!periodText.empty()) {
                    try { period = static_cast<std::size_t>(std::stoul(periodText)); }
                    catch (...) { period = 0; }
                }
                const auto rsi = calculateRsi(closes, period);
                body = rsi.ok
                    ? json({{"indicator", "RSI"}, {"period", period},
                            {"value", rsi.value}, {"interpretation", rsi.interpretation},
                            {"reason", rsi.reason}}).dump()
                    : json({{"error", rsi.reason}}).dump();
            } else if (path.rfind("/api/market-quote/ohlc?instrument_key=", 0) == 0) {
                const auto keys = instrumentKeys(queryValue(path, "instrument_key"));
                const auto quotes = client_.fetchOhlcQuotes(keys, queryValue(path, "interval"));
                body = quotes.ok ? quotes.rawBody : json({{"error", quotes.error}}).dump();
            } else if (path.rfind("/api/stock-analysis?symbol=", 0) == 0) {
                std::string symbol = path.substr(std::string("/api/stock-analysis?symbol=").size());
                std::size_t encodedAmp = symbol.find("%26");
                if (encodedAmp != std::string::npos) symbol.replace(encodedAmp, 3, "&");
                {
                    std::lock_guard<std::mutex> lock(analysisMutex_);
                    if (!stockAnalysisRunning_.count(symbol) &&
                        !stockAnalysisResults_.count(symbol) &&
                        !stockAnalysisErrors_.count(symbol)) {
                        stockAnalysisRunning_.insert(symbol);
                        std::thread([this, symbol]() {
                            try {
                                const std::string result = stockAnalysis(symbol);
                                std::lock_guard<std::mutex> lock(analysisMutex_);
                                stockAnalysisResults_[symbol] = result;
                                stockAnalysisRunning_.erase(symbol);
                            } catch (const std::exception& error) {
                                std::lock_guard<std::mutex> lock(analysisMutex_);
                                stockAnalysisErrors_[symbol] = error.what();
                                stockAnalysisRunning_.erase(symbol);
                            }
                        }).detach();
                    }
                    if (stockAnalysisRunning_.count(symbol)) {
                        body = json({{"status", "running"}}).dump();
                    } else if (stockAnalysisErrors_.count(symbol)) {
                        body = json({{"status", "error"}, {"error", stockAnalysisErrors_[symbol]}}).dump();
                    } else {
                        body = stockAnalysisResults_[symbol];
                    }
                }
            } else if (path.rfind("/api/fundamentals?symbol=", 0) == 0) {
                const std::string symbol = urlDecode(path.substr(std::string("/api/fundamentals?symbol=").size()));
                std::lock_guard<std::mutex> lock(analysisMutex_);
                if (!fundamentalsRunning_.count(symbol) && !fundamentalsResults_.count(symbol) && !fundamentalsErrors_.count(symbol)) {
                    fundamentalsRunning_.insert(symbol);
                    std::thread([this, symbol]() {
                        try {
                            const std::string result = fundamentalsAnalysis(symbol);
                            std::lock_guard<std::mutex> resultLock(analysisMutex_);
                            fundamentalsResults_[symbol] = result;
                            fundamentalsRunning_.erase(symbol);
                        } catch (const std::exception& error) {
                            std::lock_guard<std::mutex> resultLock(analysisMutex_);
                            fundamentalsErrors_[symbol] = error.what();
                            fundamentalsRunning_.erase(symbol);
                        }
                    }).detach();
                }
                if (fundamentalsRunning_.count(symbol)) body = json({{"status", "running"}}).dump();
                else if (fundamentalsErrors_.count(symbol)) body = json({{"status", "error"}, {"error", fundamentalsErrors_[symbol]}}).dump();
                else body = fundamentalsResults_[symbol];
            } else if (path == "/api/holdings" || path == "/api/positions" ||
                       path == "/api/news" || path == "/api/deeper-analysis" || path == "/metrics") {
                if (path == "/api/deeper-analysis") {
                    {
                        std::lock_guard<std::mutex> lock(analysisMutex_);
                        const bool analysisExpired = analysisCreated_ == std::chrono::steady_clock::time_point{} ||
                            std::chrono::steady_clock::now() - analysisCreated_ > std::chrono::minutes(5);
                        if (!analysisRunning_ && (analysisResult_.empty() || analysisExpired)) {
                            analysisResult_.clear();
                            analysisError_.clear();
                            analysisRunning_ = true;
                            std::thread([this]() {
                                try {
                                    const std::string result = runDeeperAnalysis();
                                    std::lock_guard<std::mutex> lock(analysisMutex_);
                                    analysisResult_ = result;
                                    analysisCreated_ = std::chrono::steady_clock::now();
                                    analysisError_.clear();
                                    analysisRunning_ = false;
                                } catch (const std::exception& error) {
                                    std::lock_guard<std::mutex> lock(analysisMutex_);
                                    analysisError_ = error.what();
                                    analysisRunning_ = false;
                                }
                            }).detach();
                        }
                        if (analysisRunning_) {
                            body = json({{"status", "running"}}).dump();
                        } else if (!analysisError_.empty()) {
                            body = json({{"status", "error"}, {"error", analysisError_}}).dump();
                        } else {
                            body = analysisResult_;
                        }
                    }
                } else {
                try {
                    const auto current = snapshot();
                    if (path == "/api/holdings") body = ensureValidJson(current->holdings);
                    else if (path == "/api/positions") body = ensureValidJson(current->positions);
                    else if (path == "/api/news") body = ensureValidJson(current->news);
                    else body = current->metrics;
                    if (path == "/metrics") {
                        const std::string output = response(body, "text/plain; version=0.0.4");
                        sendAll(connection, output);
                        close(connection); continue;
                    }
                } catch (const std::exception& e) {
                    body = json({{"error", "Snapshot error"}, {"details", e.what()}}).dump();
                }
                }
            } else if (path == "/api/config") body = json({{"status", "ready"}, {"holdings_file", holdingsFile_}}).dump();
            else body = json({{"error", "not found"}}).dump();
        } catch (const std::exception& e) {
            body = json({{"error", "Request failed"}, {"details", e.what()}}).dump();
        }
        const std::string output = response(body);
        sendAll(connection, output);
        close(connection);
    }
}

} // namespace folio
