const nbstag = (n) => '<span style="position:relative; left:-' + String(n) + 'ch">';

class Typewriter {
	constructor(container, content) {
		this.content = content;
		this.container = container;
		this.clear();
		container.addEventListener('keydown', (ev) => this.keydown(ev));
	}

	keydown(k) {
		event.preventDefault();

		let key = event.key;
		let code = -1
		if(key.length == 1)
			code = key.charCodeAt(0);
		else switch(key) {
		case "Backspace": code = 0o10; break;
		case "Tab": code = 0o11; break;
		case "Enter": code = 0o15; break;
		}

		if(code >= 0) {
	//		console.log("typed", key, code);
			ws.send(JSON.stringify({ type: 'key', value: code&0o377 }));
		}
	}

	clear() {
		this.isred = false;
		this.page = "";
		this.nbs = 0;
		this.line = "";
		this.linepos = 0;
		this.endline = "";
		this.update("_");
	}

	update(line) {
		this.content.innerHTML = this.page + line + this.endline;
		this.container.scrollTop = this.container.scrollHeight;
	}

	red() {
		this.line += '<span class="red">';
		this.endline += '</span>';
		this.isred = true;
		this.update(this.line + "_");
	}

	black() {
		this.line += '<span class="black">';
		this.endline += '</span>';
		this.isred = false;
		this.update(this.line + "_");
	}

	isspacing(c) {
		if(c == '‾' || c == '|' ||
		   c == '·' || c == '_')
			return false;
		return true;
	}

	printchar(c) {
		if(c == '\b') {
			if(this.linepos > 0) {
				this.linepos--;
				this.nbs++;
			}
			let tmpline = this.line + nbstag(this.nbs) + "_</span>" + this.endline;
			this.update(tmpline);
			return;
		}
		if(this.nbs > 0) {
			this.line += nbstag(this.nbs);
			this.endline += "</span>";
			this.nbs = 0;
		}

		if(c == '<') c = '&lt;';
		else if(c == '>') c = '&gt;';
		else if(c == '&') c = '&amp;';	// not in charset actually
		else if(c == '~') c = '˜';

		this.line += c;
		if(c == '\t')
			this.linepos = (this.linepos+8)&~7;
		else
			this.linepos++;
		if(c == '\n') {
			this.page += this.line + this.endline;
			this.line = "";
			this.endline = "";
			this.linepos = 0;
			if(this.isred)
				this.red();
		}
		if(!this.isspacing(c))
			this.printchar('\b');
		this.update(this.line + "_");
	}
}
