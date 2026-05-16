v31 WiFi + UI tweak

UI:
- display volume a 7 segmenti leggermente più piccolo
- volume spostato leggermente più in basso/centrato verticalmente
- più distanza dalla scritta input in alto
- slider/dB/preset spostati coerentemente

WiFi:
- configurata rete test:
  SSID: Rexxus_Net
  Password: impostata nel codice come WIFI_PASS
- all'avvio prova connessione STA e stampa IP su seriale:
  [WiFi] Connected, IP=...
- aggiorna RSSI ogni 5s per badge display.

Nota sicurezza:
per prodotto finito conviene spostare SSID/password in NVS o captive portal, non hardcoded.
