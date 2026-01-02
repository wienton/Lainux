#ifndef LANG_H
#define LANG_H

typedef enum {
    LANG_EN,
    LANG_RU
} Language;

extern Language current_lang;

extern const char* en_strings[][2];
extern const char* ru_strings[][2];

const char* get_text(const char* key);
void select_language(void);

#endif
