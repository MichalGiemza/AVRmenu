#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include <stdio.h>

// Stale
#define LCD_INIT_STR "\004\x64\001\x28\004\x64\001\x0C\004\x64\001\x06\004\x64\001\x01\004\x64"

// Globalne
volatile struct menu *menuptr;
volatile char *lcd_buff;
volatile unsigned char lcd_buff_full = 0;
volatile uint8_t key = 255;
volatile uint8_t ml = 0; // Menu local

// RS:PB0, EN:PB2, D4-7:PD4-7;
struct lcds {
	volatile uint8_t rs : 1;
	volatile uint8_t : 1;
	volatile uint8_t en : 1;
	volatile uint8_t : 1;
	volatile uint8_t da : 4;
};
#define lcd_rs     ((struct lcds*)&PORTB)->rs
#define lcd_rs_dir ((struct lcds*)&DDRB )->rs
#define lcd_en     ((struct lcds*)&PORTB)->en
#define lcd_en_dir ((struct lcds*)&DDRB )->en
#define lcd_da     ((struct lcds*)&PORTD)->da
#define lcd_da_dir ((struct lcds*)&DDRD )->da

struct keys {
	volatile unsigned char k : 4;
};
#define keyh     ( ( struct keys* ) &PINC ) -> k
#define keyh_dir ( ( struct keys* ) &DDRC ) -> k
#define keyh_pullup ( ( struct keys* ) &PORTC ) -> k

// Menu
struct menu {
	struct menu *g, *l, *p, *d;
	char *n;
	void(*f)(int);
};

struct menu M0, M1, M2, M3,
	M00, M01, M02, M10, M11, M20, M21, M30, M31,
	M010, M011, M012;

/*
		M0                        M1        M2       M3
		M00  M01             M02  M10  M11  M20 M21  M30 M31
			 M010 M011 M012
*/

// Funkcje menu
void f_M00(int arg) {
	switch (key) {
	case 1: // 1 gora
		ml -= 1;
		if (ml == 0) {
			key = 255;
			return;
		}
		break;
	case 2: // 2 lewo
		break;
	case 4: // 4 prawo
		ml += 4;
		break;
	case 8: // 8 dol (Enter)
		ml += 1;
		break;
	}

	if (lcd_buff_full == 0) {
		sprintf(lcd_buff, "\001\x01\004\x64Menu local: %d", ml);
		lcd_buff_full = 1;
	}

	key = 0;
}

void f_M010(int arg) {

	//static uint8_t a[5] = {32, 47, 5, 8, 9};
	//uint8_t p;

	//ukryc kursor, wypisac, przeniesc i wlaczyc
	// Normal  \x0C
	// Miganie \x0D
	// Kursor  \x0E

	switch (key) {
	case 1: // 1 gora
		ml -= 1;
		if (ml == 0) {
			key = 255;
			return;
		}
		break;
	case 2: // 2 lewo
		break;
	case 4: // 4 prawo
		break;
	case 8: // 8 dol (Enter)
		ml += 1;
		break;
	}

	if (lcd_buff_full == 0) {
		sprintf(lcd_buff,
			"\001\x01\001\x0C\004\x64ml=%d\001\%c\004\x64\001\x0D\004\x64", ml,
			0x80 + (ml == 1 ? 3 : ml == 2 ? 2 : ml == 3 ? 1 : 0));
		lcd_buff_full = 1;
	}

	key = 0;
}

// Tworzenie menu
struct menu M0 = { NULL, NULL, &M1, &M00, "M0", NULL };
struct menu M1 = { NULL, &M0,  &M2, &M10, "M1", NULL };
struct menu M2 = { NULL, &M1,  &M3, &M20, "M2", NULL };
struct menu M3 = { NULL, &M2,  &M0, &M30, "M3", NULL };

struct menu M00 = { &M0, NULL, &M01, NULL,  "M00", f_M00 };
struct menu M01 = { &M0, &M00, &M02, &M010, "M01", NULL };
struct menu M02 = { &M0, &M01, NULL, NULL,  "M02", NULL };

struct menu M10 = { &M1, &M11, &M11, NULL, "M10", NULL };
struct menu M11 = { &M1, &M10, &M10, NULL, "M11", NULL };

struct menu M20 = { &M2, NULL, &M21, NULL, "M20", NULL };
struct menu M21 = { &M2, &M20, NULL, NULL, "M21", NULL };

struct menu M30 = { &M3, NULL, &M31, NULL, "M30", NULL };
struct menu M31 = { &M3, &M30, NULL, NULL, "M31", NULL };

struct menu M010 = { &M01, &M012, &M011, NULL, "M010", f_M010 };
struct menu M011 = { &M01, &M010, &M012, NULL, "M011", NULL };
struct menu M012 = { &M01, &M011, NULL,  NULL, "M012", NULL };

// LCD
void lcd_write(uint8_t d, uint8_t rs) {
	lcd_rs = rs;

	lcd_en = 1;
	lcd_da = d >> 4;
	lcd_en = 0;

	lcd_en = 1;
	lcd_da = d;
	lcd_en = 0;
}

// Przerwanie
ISR(TIMER0_COMP_vect) {
	static unsigned char lcd_cnt = 0;
	static unsigned char lcd_r = 0;

	static unsigned char keyCnt = 0;
	static unsigned char keyLev = 0;
	static unsigned char keyr = 0;

	// Wyœwietlacz
	if (lcd_cnt == 0) {
		if (lcd_buff_full == 1) {
			switch (lcd_buff[lcd_r]) {
			case 0: // Koniec lancucha
				lcd_r = 0;
				lcd_buff_full = 0;
				break;
			case 1: // Komenda
				lcd_r++;
				lcd_write(lcd_buff[lcd_r], 0);
				lcd_r++;
				break;
			case 4: // Czekanie
				lcd_r++;
				lcd_cnt = lcd_buff[lcd_r];
				lcd_r++;
				break;
			default: // Wypisanie
				lcd_write(lcd_buff[lcd_r], 1);
				lcd_r++;
				break;
			}
		}
	}
	else
		lcd_cnt--;

	// Klawiatura
	if (keyCnt == 0) {

		switch (keyLev) {
		case 0:
			if (keyh != 15) {
				keyr = keyh;

				keyCnt = 200;
				keyLev = 1;
			}
			break;
		case 1:
			if (keyh == keyr) {
				key = ~(keyr | 0xF0);

			}
			keyLev = 2;
			break;
		case 2:
			if (keyh == 15) {
				keyCnt = 200;
				keyLev = 0;
			}
			break;

		}
	}
	keyCnt--;
}

// Menu default
void menu_default() {
	switch (key) {
	case 1: // 1 gora
		if (menuptr->g != NULL)
			menuptr = menuptr->g;
		break;
	case 2: // 2 lewo
		if (menuptr->l != NULL)
			menuptr = menuptr->l;
		break;
	case 4: // 4 prawo
		if (menuptr->p != NULL)
			menuptr = menuptr->p;
		break;
	case 8: // 8 dol (Enter)
		if (menuptr->d != NULL)
			menuptr = menuptr->d;
		else {
			if (menuptr->f != NULL) {
				ml += 1;
				key = 255;
				return;
			}
		}
		break;
	}

	if (lcd_buff_full == 0) {
		sprintf(lcd_buff, "\001\x01\004\x64%s", menuptr->n);
		lcd_buff_full = 1;
	}

	key = 0;
}

// Main
int main(void) {

	OCR0A = 199;
	TCCR0A |= 1 << WGM01;
	TCCR0B |= 1 << CS01;
	TIMSK0 |= 1 << OCIE0A;
	sei();

	lcd_rs_dir = 1;
	lcd_en_dir = 1;
	lcd_da_dir = 15;

	keyh_dir = 0;
	keyh_pullup = 15;

	lcd_buff = malloc(80);

	sprintf(lcd_buff, LCD_INIT_STR);
	lcd_buff_full = 1;

	menuptr = &M0;
	DDRC = 0xff;

	while (lcd_buff_full == 1);

	while (1) {
		if (key != 0) {

			if (ml == 0)
				menu_default();
			else
				(*(menuptr->f))(0);

		}
	}
	return 0;
}
