v35 boot mutex fix

Fix del crash:
assert failed: xQueueSemaphoreTake queue.c:1545 (( pxQueue ))
Backtrace setup() line 755

Causa:
una chiamata WiFi usava stateMutex prima della creazione del mutex.

Correzioni:
- rimossa qualunque chiamata wifi_load_config()/wifi_connect_local() all'inizio di setup().
- wifi_connect_local() ora controlla stateMutex != nullptr prima di xSemaphoreTake().
- network_task continua a caricare la config WiFi da NVS e connettersi dopo l'inizializzazione base.
- mantenuta UI con numero volume a 7 segmenti.
- mantenuto stack Display 24576.
