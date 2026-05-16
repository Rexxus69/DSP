v36 WiFi AP fallback

Perché in v35 vedevi WiFi off:
- network_task non ricaricava sempre la configurazione WiFi da NVS.
- se NVS conteneva placeholder/valori vecchi, non era evidente.
- se la connessione STA falliva, non partiva nessuna rete alternativa.

Fix:
- network_task chiama wifi_load_config() prima di WiFi.begin().
- fallback da NVS vuoto/placeholder a:
  SSID Rexxus_Net
  PWD  rebecca10
- log seriale dettagliato:
  [WiFi] config SSID=...
  [WiFi] Connecting to ...
  [WiFi] Connected, IP=...
  oppure status errore
- se non si connette, crea AP fallback:
  SSID: DSP_Controller_Setup
  PWD:  dspxlab123
  IP:   192.168.4.1
- display mostra "AP mode" se STA non è collegato ma AP fallback è attivo.

Nota:
ESP32-S3 supporta solo WiFi 2.4 GHz. Se Rexxus_Net è solo 5 GHz, non si collegherà.
