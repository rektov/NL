#pragma once
#include <cstddef>

#if defined(NL_USE_MUSEO)
extern const unsigned char MUSEO_SANS_CYRL_500_FONT[];
extern const size_t MUSEO_SANS_CYRL_500_FONT_SIZE;
extern const unsigned char MUSEO_SANS_CYRL_900_FONT[];
extern const size_t MUSEO_SANS_CYRL_900_FONT_SIZE;

#define UI_FONT_REGULAR MUSEO_SANS_CYRL_500_FONT
#define UI_FONT_REGULAR_SIZE MUSEO_SANS_CYRL_500_FONT_SIZE
#define UI_FONT_BOLD MUSEO_SANS_CYRL_900_FONT
#define UI_FONT_BOLD_SIZE MUSEO_SANS_CYRL_900_FONT_SIZE
#else
extern const unsigned char UI_FONT_REGULAR[];
extern const size_t UI_FONT_REGULAR_SIZE;
extern const unsigned char UI_FONT_BOLD[];
extern const size_t UI_FONT_BOLD_SIZE;
#endif
