
#ifndef LANG_H
#define LANG_H

typedef enum {
    LANG_EN = 0,
    LANG_RU = 1
} Language;

extern Language current_lang;

#define _(key) get_text(#key)

const char* get_text(const char* key);

void select_language(void);

#endif
