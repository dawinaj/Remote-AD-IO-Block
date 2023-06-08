#pragma once
#include "settings.h"

//

#define PORT 8867
#define KEEPALIVE_IDLE 5
#define KEEPALIVE_INTERVAL 60
#define KEEPALIVE_COUNT 3

//

template <int>
void connection_hdlr_task(void *pvParameters);
