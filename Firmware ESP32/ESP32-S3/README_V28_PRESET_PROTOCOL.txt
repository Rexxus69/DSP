v28 preset protocol

Verifica:
Il protocollo preset esisteva già:
- WS_OP_PRESET_LOAD = 0x13
- WS_OP_PRESET_SAVE = 0x14
- Frame legacy save:
  [0]=0x14
  [1]=idx
  [2..17]=name 16 byte
  [18]=relay1
  [19]=relay2

Limite trovato:
Il campo "seconda riga"/descrizione non veniva mai ricevuto né salvato,
anche se Preset_t aveva già un buffer char relays[64] inutilizzato per questo scopo.

Implementazione:
- WS_OP_PRESET_SAVE ora accetta frame esteso:
  [20..] = descrizione ASCII / seconda riga preset, max 63 byte
- Compatibile col frame legacy: se [20..] manca, conserva la descrizione già salvata.
- La descrizione viene salvata in NVS insieme al preset.
- WS_OP_PRESET_LOAD ora:
  - imposta active_preset
  - aggiorna state.pcf1_out / state.pcf2_out
  - salva l'active_preset in NVS
  - fa broadcast dello stato.
- Display:
  - preset list laterale resta PRESET 1..6
  - centro mostra "Preset attivo: <descrizione>"
  - fallback preset 1 = "3 vie stereo" se la descrizione non è ancora stata salvata.

Esempio frame esteso:
uint8_t frame[] = {
  0x14, idx,
  'P','R','E','S','E','T',' ','1',0,0,0,0,0,0,0,0,
  relay1, relay2,
  '3',' ','v','i','e',' ','s','t','e','r','e','o',0
};
