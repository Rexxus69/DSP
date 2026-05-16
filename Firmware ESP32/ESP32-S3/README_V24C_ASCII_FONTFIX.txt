v24c ASCII font fix

Corregge i rettangoli al posto dei trattini/separatori:
- sostituiti i separatori Unicode "·" con "-"
- sostituito "∞" con "-inf"
- rimossi caratteri Unicode non garantiti dal font LVGL Montserrat incluso

Il problema era il subset font LVGL: alcuni glifi non sono compilati nel font.
