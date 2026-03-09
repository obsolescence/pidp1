#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <ctype.h>
#include <assert.h>

typedef uint32_t word;
#define nil NULL

enum
{
	None = 0,
	W = 0777777,
//	MAXMEM = 0010000,	// 4k
	MAXMEM = 0200000,	// 64k extended
	AMSK = MAXMEM-1,
	SYMLEN = 10,		// currently has to fit also pseudo symbols
	MAXSYM = 4000,
	Addr = 02000000,
	Undef = 04000000
};

int chgfield(word a, word b) { return (a&0170000) != (b&0170000); }

enum PsiType
{
	PsiStart = 1,
	PsiOctal,
	PsiDecimal,
	PsiCharacter,
	PsiFlexo,
	PsiText,
	PsiExpunge,
	PsiConstants,
	PsiVariables,
	// missing:
	// 	noinput
	// 	repeat
	// 	dimension
	// 	define
	// 	terminate
};

enum TokType
{
	TokEol = 0200,
	TokPseudo,
	// syllable
	TokSymbol,
	TokWord,
};

typedef struct Token Token;
typedef struct Sym Sym;
typedef struct Word Word;
typedef struct Contab Contab;

struct Token
{
	int type;
	union {
		Sym *sym;
		word w;
		int psi;
	};
};

struct Sym
{
	char name[SYMLEN];
	word val;
	int supress;
};

struct Contab
{
	word start, end;
	int size;	// includes wrongly allocated ones
};

FILE *infp, *outfp, *lstfp, *tmpfp;
Sym symtab[MAXSYM];
Sym *dot;
word startaddr;
int radix = 8;
char *filename;
int lineno = 1;
int pass2;
int lastlistline = ~0;
char line[80];
int peekc;
Token peekt;

int bin, ext;
word loadaddr;
word memory[MAXMEM];
Contab cons[10];
Contab *conp;
int ncon;

void
err(int n, char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	fprintf(stderr, "%s:%d: pass %d: ", filename, lineno, pass2+1);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	va_end(ap);

	if(n)
		exit(1);
}

char*
ssprintf(char *fmt, ...)
{
	static char buf[1024];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	return buf;
}

void
panic(char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	va_end(ap);
	exit(1);
}

FILE*
mustopen(const char *name, const char *mode)
{
	FILE *f;
	if(f = fopen(name, mode), f == nil)
		panic("couldn't open file: %s", name);
	return f;
}

word
add(word w1, word w2)
{
	w1 = (w1&W) + (w2&W);
	if(w1 & 01000000) w1 += 1;
	return w1 & W;
}

word
punchw(word w)
{
	fputc((w>>12)&077 | 0200, outfp);
	fputc((w>>6)&077 | 0200, outfp);
	fputc((w>>0)&077 | 0200, outfp);
	return w;
}

void
rimword(word a, word w)
{
	punchw(a | 0320000);
	punchw(w);
}

void
feed(int n)
{
	n *= 3;
	while(n--)
		fputc(0, outfp);
}

void
begintape(void)
{
	word a;

	feed(10);
	if(bin) {
		a = loadaddr;
		if(ext) {
			rimword(a++, 0724074);	//      go,     eem
			rimword(a++, 0730002);	//              rpb     / DIO / JMP
			rimword(a++, 0327760);	//              dio a
			rimword(a++, 0730002);	//              rpb     / start
			rimword(a++, 0327776);	//              dio ck
			rimword(a++, 0320000);	//              dio 0
			rimword(a++, 0107760);	//              xct a   / execute JMP
			rimword(a++, 0730002);	//              rpb     / was DIO - end
			rimword(a++, 0327777);	//              dio en
			rimword(a++, 0730002);	//      b,      rpb     / data word
			rimword(a++, 0760400);	//      a,      xx      / DIO i 0
			rimword(a++, 0210000);	//              lac i 0
			rimword(a++, 0407776);	//              add ck
			rimword(a++, 0247776);	//              dac ck
			rimword(a++, 0440000);	//              idx 0
			rimword(a++, 0527777);	//              sas en
			rimword(a++, 0607757);	//              jmp b
			rimword(a++, 0207776);	//              lac ck
			rimword(a++, 0407777);	//              add en
			rimword(a++, 0730002);	//              rpb     / sum
			rimword(a++, 0327776);	//              dio ck
			rimword(a++, 0527776);	//              sas ck
			rimword(a++, 0760400);	//              hlt
			rimword(a++, 0607746);	//              jmp go
		} else {
			rimword(a++, 0730002);
			rimword(a++, 0327760);
			rimword(a++, 0107760);
			rimword(a++, 0327776);
			rimword(a++, 0730002);
			rimword(a++, 0327777);
			rimword(a++, 0730002);
			rimword(a++, 0000000);
			rimword(a++, 0217760);
			rimword(a++, 0407776);
			rimword(a++, 0247776);
			rimword(a++, 0447760);
			rimword(a++, 0527777);
			rimword(a++, 0607757);
			rimword(a++, 0207776);
			rimword(a++, 0407777);
			rimword(a++, 0730002);
			rimword(a++, 0327776);
			rimword(a++, 0527776);
			rimword(a++, 0760400);
			rimword(a++, 0607751);
		}
		punchw(0600000 | loadaddr);
		feed(10);
	}
}

word bufbg = ~0;
word buf[0100];
int nbuf;

void
flushbuf(void)
{
	word a = bufbg;
	if(nbuf == 0) return;
	punchw(0330000);	// mark data block
	word sum = 0;
	sum = add(sum, punchw(bufbg));
	sum = add(sum, punchw(bufbg+nbuf));
	for(int i = 0; i < nbuf; i++)
		sum = add(sum, punchw(buf[i]));
	punchw(sum);
	feed(5);
	nbuf = 0;
}

void
outword(word a, word w)
{
	if(bin) {
		if(bin && a >= loadaddr && a < 010000)
			err(1, "error: overflow into BIN loader\n");
		if(nbuf == 0100 || a != bufbg+nbuf) {
			flushbuf();
			bufbg = a;
		}
		buf[nbuf++] = w;
	} else {
		rimword(a, w);
	}
}

void
endtape(int start)
{
	flushbuf();
	feed(10);
	if(ext) {
		punchw(0610000);
		punchw(start);
	} else
		punchw(start | 0600000);
	feed(10);
}

void
list(char *left)
{
	if(left == nil)
		left = "          \t";
	if(line[0])
		fprintf(lstfp, "%s%05d    %s", left, lastlistline, line);
	else
		fprintf(lstfp, "%s\n", left);
	line[0] = '\0';
}

int
cmpsym(const void *s1, const void *s2)
{
	return strcmp(((Sym*)s1)->name, ((Sym*)s2)->name);
}

void
listsyms(void)
{
	Sym *s;

	qsort(symtab, MAXSYM, sizeof(Sym), cmpsym);
	fprintf(lstfp, "\n    SYMBOL TABLE\n");
	for(s = symtab; s < &symtab[MAXSYM]; s++) {
		if(s->name[0] == '\0' || s->supress)
			continue;
		fprintf(lstfp, "%-7s %06o\n", s->name, s->val & W);
	}
}

void
unch(int c)
{
	if(c == '\n') lineno--;
	peekc = c;
}

int
ch(void)
{
	int c;

	if(peekc) {
		c = peekc;
		peekc = 0;
	} else {
		/* listing */
		if(pass2 && lastlistline != lineno) {
			/* if line hasn't been printed yet, do it now */
			if(line[0])
				list(nil);

			int pos = ftell(infp);
			char *p;
			for(p = line; p < &line[80-2]; p++) {
				c = getc(infp);
				if(c == '\n' || c == EOF)
					break;
				*p = c;
			}
			if(!(p == line && c == EOF))
				*p++ = '\n';
			*p++ = '\0';
			fseek(infp, pos, SEEK_SET);
			lastlistline = lineno;
		}

		c = getc(infp);
		if(c == EOF)
			return EOF;
		// We only want ASCII!!
		c &= 0177;
		if(tmpfp)
			putc(c, tmpfp);
	}
	if(c == '\n')
		lineno++;
	return c;
}

int
peek(void)
{
	unch(ch());
	return peekc;
}

// like ch() but skip whitespace except newlines	
int
chsp(void)
{
	int c;
	while(c = ch(),
		c == ' ' || c == '\r' || c == '\v');
	return c;
}

int
peeksp(void)
{
	unch(chsp());
	return peekc;
}

// translation table
// fiodec	ascii
// 204  ⊃	#
// 205  ∨	!
// 206  ∧	&
// 220  →	\
// 273  ×	*
// 140  ·	@
// 156  ‾	`
static int ascii2fio[] = {
	  -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
	0075, 0036,   -1,   -1,   -1, 0077,   -1,   -1,
	  -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
	  -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,

	0000, 0205, 0201, 0204,   -1,   -1, 0206, 0202,
	0157, 0155, 0273, 0254, 0133, 0154, 0173, 0121,
	0120, 0101, 0102, 0103, 0104, 0105, 0106, 0107,
	0110, 0111,   -1,   -1, 0207, 0233, 0210, 0221,

	0140, 0261, 0262, 0263, 0264, 0265, 0266, 0267,
	0270, 0271, 0241, 0242, 0243, 0244, 0245, 0246,
	0247, 0250, 0251, 0222, 0223, 0224, 0225, 0226,
	0227, 0230, 0231, 0257, 0220, 0255, 0211, 0240,

	0156, 0161, 0162, 0163, 0164, 0165, 0166, 0167,
	0170, 0171, 0141, 0142, 0143, 0144, 0145, 0146,
	0147, 0150, 0151, 0122, 0123, 0124, 0125, 0126,
	0127, 0130, 0131,   -1, 0256,   -1, 0203,   -1,
};

// TODO: handle uc/lc better
int chfio(void) { return ascii2fio[ch()]&077; }

/* Symbols */

/* Just find symbol, or return nil. */
Sym*
findsym(char *name)
{
	Sym *s;
	for(s = symtab; s < &symtab[MAXSYM]; s++)
		if(strncmp(s->name, name, SYMLEN) == 0)
			return s;
	return nil;
}

/* Get symbol, add to table if not present */
Sym*
getsym(char *name)
{
	Sym *s;
	s = findsym(name);
	if(s == nil) {
		for(s = symtab; s < &symtab[MAXSYM]; s++)
			if(s->name[0] == 0)
				goto found;
		panic("symbol table full");
found:
		strncpy(s->name, name, SYMLEN);
		s->val = Undef;
		s->supress = 0;
	}
	return s;
}

/* Parser */

int
isdelim(int c)
{
	// actually not ,/)=
	return strchr("+- \t\n/,)=", c) != nil;
}

int pseudo(char *sym);

int sylhack;

Token
syllable(void)
{
	Token t;
	char symbuf[SYMLEN+1];
	int c;
	int len;
	int let;
	int n;

	len = 0;
	memset(symbuf, 0, sizeof(symbuf));
	let = 0;
	n = 0;
	while(c = ch(), !isdelim(c)) {
		if(isalpha(c) || isdigit(c)) {
			if(len<SYMLEN)
				symbuf[len++] = tolower(c);
			if(isalpha(c)) let |= 1;
			else if(isdigit(c)) n = n*radix + c-'0';
		} else
			let |= 2;
	}
	unch(c);
	if(let) {
		if(pseudo(symbuf)) {
			// make token() try again
			t.type = None;
			return t;
		} else {
			t.type = TokSymbol;
			t.sym = getsym(symbuf);
		}
	} else {
		t.type = TokWord;
		t.w = n;
	}
	sylhack = 1;	// so / can work correctly
	return t;
}

Token
token(void)
{
	static char *self = "+-!,=$*&()[]<>";
	int c;
	Token t;

tail:
	if(peekt.type != None) {
		t = peekt;
		peekt.type = None;
		return t;
	}

	while(c = chsp(), c != EOF) {
		int hack = sylhack;
		sylhack = 0;
		if(isdigit(c) || isalpha(c)) {
			unch(c);
			t = syllable();
			if(t.type == None)
				goto tail;
			return t;
		} else if(strchr(self, c)) {
			t.type = c;
			return t;
		} else switch(c) {
		case '.':
			t.type = TokSymbol;
			t.sym = dot;
			return t;

		case '\t':
		case '\n':
			t.type = TokEol;
			return t;

		case '/':
			if(hack) {
				t.type = '/';
			} else {
				while(ch() != '\n');
				t.type = TokEol;
			}
			return t;
		default:
			err(0, "warning: ignored character %c", ch());
		}
	}
	t.type = TokEol;
	return t;
}

/* Assembler */

void setdot(word w) { dot->val = w&AMSK | Addr; }

void
writecons(void)
{
	word a;
	static char left[64];

	if(!pass2) {
		conp[1].start = conp[1].end = 0;
		conp->size = conp->end - conp->start;
		conp->start += dot->val&AMSK;
		// for pass2
		conp->end = conp->start;
	} else {
		for(a = conp->start; a < conp->end; a++) {
			// listing
			sprintf(left, "%06o  %06o\t", a, memory[a]);
			list(left);

			outword(a, memory[a]);
		}
	}

	if(chgfield(dot->val, dot->val+conp->size-1))
		err(1, "err: constant area overflowed page");
	setdot(dot->val+conp->size);
	conp++;
}

word
putcon(word w)
{
	word a;

	// check if found
	if(w != Undef) {
		w &= W;
		for(a = conp->start; a < conp->end; a++)
			if(memory[a] == w)
				return a;
	}
	memory[conp->end] = w;
	return conp->end++ | Addr;
}

/* write out a word */
void
putword(word w)
{
	static char left[64];

	w &= W;
	if(pass2) {
		/* listing */
		sprintf(left, "%06o  %06o\t", dot->val&AMSK, w);
		list(left);

		outword(dot->val&AMSK, w);
	}
	if((dot->val&W) > 07777) ext = 1;
	if(chgfield(dot->val, dot->val+1))
		writecons();
	setdot(dot->val+1);
}

word
addrof(word w)
{
	if(w & Addr) {
		if(pass2 && chgfield(w, dot->val))
			err(0, "warning: field mismatch %06o %06o\n", dot->val&W, w&W);
		// we strip off the address bit here too, not sure if ideal
		w &= 07777;
	}
	return w;
}

word
combine(int op, word w1, word w2)
{
	if(op != 0 && w1 == Undef || w2 == Undef) {
		if(pass2)
			err(1, "error; undefined value %o %o", w1, w2);
		return Undef;
	}
	// heuristic:
	// restrict addresses to 12 bits when building instructions
	if(op == ' ') {
		if(w1 & Addr) w1 = addrof(w1);
		if(w2 & Addr) w2 = addrof(w2);
	}
	switch(op) {
	case 0:
		return w2;
	case '+':
		w1 = add(w1, w2);
		break;
	case '-':
		w1 = add(w1, w2^W);
		break;
	case ' ':	// MACRO does the same as + actually, but OR is more useful
	case '!':
		w1 |= w2;
		break;
	case '&':
		w1 &= w2;
		break;
	default:
		panic("unknown operator %c", op);
	}
	return w1 & (Addr|W);
}

word tokword(Token t) { return t.type==TokWord ? t.w : t.sym->val; }

word
expr(void)
{
	word w, v;
	Token t;
	int op;

	/* operators:
	 * + add
	 * - sub
	 * ! or
	 * & and
	 * space combine */

	op = 0;
	w = 0;
	// not quite sure about expression delimiter here...
	while(t = token(), t.type != TokEol &&
	      t.type != ')' && t.type != ']' && t.type != ',' && t.type != '/') {
		switch(t.type) {
		case TokWord:
		case TokSymbol:
			w = combine(op, w, tokword(t));
			op = ' ';
			break;

		case '(':
			v = expr();
			v = putcon(v);
			w = combine(op, w, v);
			op = ' ';
			t = token();
			if(t.type != ')') {
				if(t.type != TokEol)
					err(1, "err: literal not closed");
				peekt = t;
			}
			break;

		case '+':
		case '-':
		case '!':
		case '&':
			op = t.type;
			break;
		}
	}
	peekt = t;
	return w;
}

struct {
	char *str;
	int psi;
} psi[] = {
	 { "start", PsiStart },
	 { "octal", PsiOctal },
	 { "decimal", PsiDecimal },
	 { "character", PsiCharacter },
	 { "char", PsiCharacter },
	 { "flexo", PsiFlexo },
	 { "text", PsiText },
	 { "expunge", PsiExpunge },
	 { "constants", PsiConstants },
	 { "variables", PsiVariables },
	 { nil, -1 }
};


int
pseudo(char *sym)
{
	int a, c, d, i;
	word w;

	int n = strlen(sym);
	if(n < 4) return 0;
	int best = 0;
	for(int i = 0; psi[i].str != nil; i++) {
//		if(strncmp(psi[i].str, sym, n) == 0)
		// actually let's check for exact matches only
		// even if this is slightly incompatible
		if(strcmp(psi[i].str, sym) == 0)
			best = psi[i].psi;
	}
	if(best == 0) return 0;

	switch(best) {
	case PsiStart:
		w = expr();
		startaddr = w & W;
		unch(EOF);	// end assembly
		break;

	case PsiOctal:
		radix = 8;
		break;

	case PsiDecimal:
		radix = 10;
		break;

	case PsiCharacter:
		peekc = 0;
		a = ch();
		w = chfio();
		if(a == 'm') w <<= 6;
		else if(a == 'l') w <<= 12;
		else if(a != 'r') err(0, "warning: unknown spec %c\n", a);
	wd:
		peekt.type = TokWord;
		peekt.w = w;
		break;

	case PsiFlexo:
		// three chars L->R
		peekc = 0;
		w = chfio(); w <<= 6;
		w |= chfio(); w <<= 6;
		w |= chfio();
		goto wd;

	case PsiText:
		// delimited text, 3chars per word
		peekc = 0;
		w = 0;
		d = chfio();
		i = 0;
		while(c = chfio(), c != d) {
			w = (w<<6) | c;
			if(++i == 3) {
				putword(w);
				i = 0;
				w = 0;
			}
		}
		break;

	case PsiExpunge:
		// TODO
		break;
	case PsiConstants:
		writecons();
		break;
	case PsiVariables:
		// TODO
		break;
	}

	return 1;
}

void
statement(void)
{
	Token t;
	Sym *s;
	word w;

	while(t = token(), t.type != TokEol) {
		switch(t.type) {
		case TokSymbol:
			s = t.sym;
			if(peek() == ',') {
				peekc = 0;
				if(s->val != Undef && s->val != dot->val)
					err(1, "error: label %s redefinition %o %o",
					    s->name, s->val, dot->val);
				s->val = dot->val;
			} else if(peek() == '=') {
				peekc = 0;
				w = expr();
				s->val = w;
			} else
				goto xpr;
			break;

		case TokWord:
		case '-': case '(':
		xpr:
			peekt = t;
			w = expr();
			if(peekt.type == '/') {
				peekt.type = None;
				if(w == Undef)
					err(1, "error: undefined location assignment");
				if(chgfield(dot->val, w))
					writecons();
				setdot(w);
			} else
				putword(w);
			break;

		default:
			err(1, "unknown token %c", t.type);
		}
	}
}

void
assemble(void)
{
	conp = cons;
	sylhack = 0;
	peekc = 0;
	peekt.type = None;
	while(peek() != EOF)
		statement();

	// list last line
	if(pass2 && line[0])
		list(nil);

	writecons();
}

void
checkundef(void)
{
	int i;
	int e;

	e = 0;
	for(i = 0; i < MAXSYM; i++)
		if(symtab[i].name[0] && symtab[i].val == Undef) {
			err(0, "error: %s undefined", symtab[i].name);
			e = 1;
		}
	if(e)
		err(1, "errors in first pass");
}

/* Init */

struct
{
	char *sym;
	word val;
} valtab[] = {
	 { ".", 0 },
	 { "i", 0010000 },
	 { "and", 0020000 },
	 { "ior", 0040000 },
	 { "xor", 0060000 },
	 { "xct", 0100000 },
	 { "jfd", 0120000 },
	 { "cal", 0160000 },
	 { "jda", 0170000 },
	 { "lac", 0200000 },
	 { "lio", 0220000 },
	 { "dac", 0240000 },
	 { "dap", 0260000 },
	 { "dip", 0300000 },
	 { "dio", 0320000 },
	 { "dzm", 0340000 },
	 { "add", 0400000 },
	 { "sub", 0420000 },
	 { "idx", 0440000 },
	 { "isp", 0460000 },
	 { "sad", 0500000 },
	 { "sas", 0520000 },
	 { "mus", 0540000 },
	 { "dis", 0560000 },
	 { "mul", 0540000 },
	 { "div", 0560000 },
	 { "jmp", 0600000 },
	 { "jsp", 0620000 },
	 { "spi", 0642000 },
	 { "szo", 0641000 },
	 { "sma", 0640400 },
	 { "spa", 0640200 },
	 { "sza", 0640100 },
	 { "szf", 0640000 },
	 { "szs", 0640000 },

	 { "ral", 0661000 },
	 { "rar", 0671000 },
	 { "ril", 0662000 },
	 { "rir", 0672000 },
	 { "rcl", 0663000 },
	 { "rcr", 0673000 },
	 { "sal", 0665000 },
	 { "sar", 0675000 },
	 { "sil", 0666000 },
	 { "sir", 0676000 },
	 { "scl", 0667000 },
	 { "scr", 0677000 },
	 { "law", 0700000 },
	 { "nop", 0760000 },
	 { "opr", 0760000 },
	 { "cli", 0764000 },
	 { "lat", 0762200 },
	 { "cma", 0761000 },
	 { "hlt", 0760400 },
	 { "cla", 0760200 },
	 { "lap", 0760300 },
	 { "clf", 0760000 },
	 { "stf", 0760010 },
	 { "1s", 01 },
	 { "2s", 03 },
	 { "3s", 07 },
	 { "4s", 017 },
	 { "5s", 037 },
	 { "6s", 077 },
	 { "7s", 0177 },
	 { "8s", 0377 },
	 { "9s", 0777 },

	 { "iot", 0720000 },
	 { "tyi", 0720004 },
	 { "rrb", 0720030 },
	 { "cks", 0720033 },
	 { "lsm", 0720054 },
	 { "esm", 0720055 },
	 { "cbs", 0720056 },
	 { "rpa", 0730001 },
	 { "rpb", 0730002 },
	 { "tyo", 0730003 },
	 { "ppa", 0730005 },
	 { "ppb", 0730006 },
	 { "dpy", 0730007 },
	 { "lem", 0720074 },
	 { "eem", 0724074 },

	 { nil, 0 }
};

void
initsymtab(void)
{
	int i;
	Sym *s;

	for(i = 0; valtab[i].sym; i++) {
		s = getsym(valtab[i].sym);
		s->supress = 1;
		s->val = valtab[i].val;
	}
	dot = findsym(".");
	dot->supress = 1;
}

int
main(int argc, char *argv[])
{
	const char *basename;

	if(argc > 1) {
		filename = argv[1];
		infp = fopen(filename, "r");
		if(infp == nil) {
			fprintf(stderr, "can't open file %s\n", filename);
			return 1;
		}
		basename = strdup(filename);
		char *p = strrchr(basename, '.');
		if(p) *p = '\0';
	} else {
		infp = stdin;
		filename = "<stdin>";
		basename = "out";
	}
	tmpfp = tmpfile();
	bin = 1;
	ext = 1;

	initsymtab();

	setdot(4);
	pass2 = 0;
	assemble();

	checkundef();
	bin |= ext;
	loadaddr = ext ? 07746 : 07751;

	pass2 = 1;
	infp = tmpfp;
	tmpfp = nil;
	rewind(infp);
	outfp = mustopen(ssprintf("%s.rim", basename), "wb");
	lstfp = mustopen(ssprintf("%s.lst", basename), "w");
	setdot(4);
	lineno = 1;
	begintape();
	assemble();
	endtape(startaddr);

	fclose(infp);
	fclose(outfp);

	listsyms();

	return 0;
}
