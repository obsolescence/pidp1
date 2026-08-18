#ifdef UNITY_BUILD

enum { 
	SE = 240,
	NOP = 241,
	BRK = 243,
	IP = 244,
	AO = 245,
	AYT = 246,
	EC = 247,
	EL = 248,
	GA = 249,
	SB = 250,
	WILL = 251,
	WONT = 252,
	DO = 253,
	DONT = 254,
	IAC = 255,

	XMITBIN = 0,
	ECHO_ = 1,
	SUPRGA = 3,
	LINEEDIT = 34,
};

int typewriterUpdated;

void
drawGlyph(int i, float x, float y)
{
	Glyph *g = &glyphs[i];
	float x1 = x/drawW;
	float y1 = y/drawH;
	float x2 = (x+g->w)/drawW;
	float y2 = (y+g->h)/drawH;
	Vertex quad[] = {
		{ x1, y1,		0.0f, 1.0f },
		{ x2, y1,		1.0f, 1.0f },
		{ x2, y2,		1.0f, 0.0f },

		{ x1, y1,		0.0f, 1.0f },
		{ x2, y2,		1.0f, 0.0f },
		{ x1, y2,		0.0f, 0.0f },
	};
	glUseProgram(tex_program);
	setvbo(immVbo);
	glBindTexture(GL_TEXTURE_2D, g->tex);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_DYNAMIC_DRAW);
	glUniform4f(glGetUniformLocation(tex_program, "u_color"),
		col.r/255.0f, col.g/255.0f, col.b/255.0f, col.a/255.0f);
	glDrawArrays(GL_TRIANGLES, 0, 6);
}

int
drawString(const char *s, int x, int y)
{
	int space = glyphs[' '].advance;
	int lx = 0;
	for(; *s; s++) {
		int c = *s;
		if(c & 0200)
			setColor(170,0,0,255);
		else
			setColor(0,0,0,255);
		c &= 0177;

		switch(c) {
		case '\b':
			lx -= space;
			if(lx < 0) lx = 0;
			break;

		case '\t':
			int nsp = lx/space;
			nsp = ((nsp+8)/8)*8;
			lx = nsp*space;
			break;

		case 020:	// mid dot
		case 021:	// overline
		case '|':
		case '_':
			drawGlyph(c, x+lx, y);
			break;

		default:
			drawGlyph(c, x+lx, y);
			lx += glyphs[c].advance;
			break;
		}
	}
	return lx;
}

enum {
	NUMLINES = 200,
	LINELEN = 100,
};
char typ_lines[NUMLINES][LINELEN];
int nlines;
int curline;
int red = 0;

void
drawTypewriter(Region *r)
{
	setRegion(r);
	setColor(255, 255, 255, 255);
	drawRectangle(0.0f, 0.0f, drawW, drawH);

	drawW = winW;
	drawH = winH;
	glViewport(0.0f, 0.0f, drawW, drawH);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glScissor(r->x, drawH-r->y-r->h, r->w, r->h);
	glEnable(GL_SCISSOR_TEST);

	int spacing = fontsize+1;
	int x = r->x + 5;
	int y = r->y+r->h - 10 - spacing;
	int lx;
	for(int i = nlines-1; i >= 0; i--)
		lx = drawString(typ_lines[(curline+NUMLINES-i)%NUMLINES], x, y-i*spacing);
	if(red)
		setColor(170,0,0,255);
	else
		setColor(128,128,128,255);
	drawGlyph('_', x+lx, y);

	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_BLEND);
}

void
initTypewriter(void)
{
	curline = 0;
	typ_lines[curline][0] = 0;
	nlines = 1;
	typewriterUpdated = 1;
}

int
mapchar(int r)
{
	switch(r) {
	case 0x00B7:	return 020;	//	·	middle dot
	case 0x203E:	return 021;	//	‾	overline
	case 0x2192:	return 022;	//	→	right arrow
	case 0x2283:	return 023;	//	⊃	superset
	case 0x2228:	return 024;	//	∨	or
	case 0x2227:	return 025;	//	∧	and
	case 0x2191:	return 026;	//	↑	up arrow
	case 0x00D7:	return 027;	//	×	times
	}
	if(r >= 128)
		return ' ';
	return r;
}

void
typeChar(int c)
{
	if((c&0177) == '\n') {
		curline = (curline+1)%NUMLINES;
		typ_lines[curline][0] = 0;
		if(nlines < NUMLINES)
			nlines++;
		typewriterUpdated = 1;
		return;
	}
	char *line = typ_lines[curline];
	int i;
	for(i = 0; line[i] != 0; i++);
	line[i++] = c;
	line[i] = 0;
	typewriterUpdated = 1;
}

void
typeString(const char *s)
{
	while(*s)
		typeChar(*s++);
}

char escstr[20];
int escp = 0;

int
leading1s(int byte) {
	int bit = 0200;
	int n = 0;
	while(bit & byte) {
		n++;
		bit >>= 1;
	}
	return n;
}

char utf8str[10];
int utf8p = 0;
int utf8len = 0;

int
torune(char *s)
{
	int r;
	int nc = leading1s(*s);
	r = *s++ & (0177>>nc--);
	while(nc--) {
		r <<= 6;
		r |= *s++ & 077;
	}
	return r;
}

void
recvChar(int c)
{
	if(c == '\r')
		return;
	if(c == 033) {
		escp = 0;
		escstr[escp++] = c;
		return;
	}
	if(escp) {
		escstr[escp++] = c;
		escstr[escp] = 0;
		if(strcmp(escstr, "\033[31m") == 0) {
			red = 0200;
			escp = 0;
		} else if(strcmp(escstr, "\033[39;49m") == 0) {
			red = 0;
			escp = 0;
		}
		return;
	}

	// this UTF-8 nonsense is really annoying
	int nc = leading1s(c);
	if(nc == 0)
		typeChar(c | red);
	else if(nc > 1) {
		// utf8 starter
		utf8len = nc;
		utf8p = 0;
		utf8str[utf8p++] = c;
	} else if(nc == 1) {
		// utf8 char
		utf8str[utf8p++] = c;
		if(utf8p == utf8len)
			typeChar(mapchar(torune(utf8str)) | red);
	}
}

int
telnetchar(int state, char cc)
{
	int c = cc & 0377;
	switch(state) {
	case 0:
		if(c == IAC)
			return IAC;
		recvChar(c);
		break;

	case WILL: case WONT: case DO: case DONT:
		// ignore c
		break;

	default:
		switch(c) {
		case NOP: break;
		case WILL: case WONT: case DO: case DONT:
			return c;
		case IAC:
			recvChar(c);
			break;
		default:
			printf("unknown cmd %x %o\n", c, c);
		}
	}
	return 0;
}

void*
typthread(void *args)
{
	int n;
	char buf[20];
	int state = 0;
	while(n = read(typfd, buf, sizeof(buf)), n >= 0) {
		for(int i = 0; i < n; i++)
			state = telnetchar(state, buf[i]);
	}
}

void
strikeChar(int c)
{
	char b = c;
	if(b == '\n')
		b = '\r';
	write(typfd, &b, 1);
}

int
doDrawTypewriter(void)
{
	int d = typewriterUpdated;
	typewriterUpdated = 0;
	return d;
}

#endif
