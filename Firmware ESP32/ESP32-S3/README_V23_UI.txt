DSP Controller integrated v23 UI final

Modifiche rispetto alla v22:
- font laterali input/preset ingranditi
- VU meter più larghi
- lista preset a destra con una sola riga: PRESET 1..6
- descrizione/configurazione del preset attivo spostata solo al centro sotto il dB
- matrice relay 2x4:
  riga alta: TW L | MID L | WF L | SW L
  riga bassa: TW R | MID R | WF R | SW R
- backlight e driver LCD restano quelli già validati:
  GPIO6 LOW = backlight ON
  pannello fisico 320x820 ruotato in UI 820x320

Nota:
Nel firmware attuale AppState/Preset_t non ha un campo desc dedicato come l'HTML.
Per la descrizione centrale viene usato p.relays se presente; altrimenti p.name/fallback.
Se vuoi una gestione pulita della seconda riga preset, il prossimo step è aggiungere:
  char desc[32];
in Preset_t e aggiornare WS_OP_PRESET_SAVE.
