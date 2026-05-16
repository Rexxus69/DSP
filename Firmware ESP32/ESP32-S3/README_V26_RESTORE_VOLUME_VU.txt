v26 restore volume + VU tick

Fix rispetto a v25:
- rimosso zoom LVGL del volume: su questo display faceva sparire il numero centrale
- volume torna a Montserrat 48 stabile e visibile
- VU meter inizializzati a 5% e refresh minimo 5%, così la prima tacca bassa resta visibile
- mantiene fix glyph ASCII
- mantiene preset centrale con descrizione ("3 vie stereo" fallback per preset 1)
- mantiene relay matrix TW/MID/WF/SW, riga L sopra e R sotto

Se si vuole volume più grande, serve font custom numerico 64/72 px, non zoom LVGL.
