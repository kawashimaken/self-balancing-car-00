#pragma once

// setup() で一度呼び、Wi-Fi AP と Core 0 の Web UI タスクを起動。
void webUiBegin();

// 空関数。Web UI は Core 0 のタスクで処理。
void webUiHandle();
