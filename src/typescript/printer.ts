import {addPrinterCssFontToPage, PRINTER_REGULAR_FONT_FAMILY} from "./printer_fonts.js";

// Holes on sides. See github.com/lkesteloot/trs80/blob/master/packages/trs80-emulator-web/assets/README.md
const BACKGROUND_LEFT_URL = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAB4AAAAeCAQAAACROWYpAAABH0lEQVQ4y+XUvU7DMBhG4cdJoECFQPwIRjbukAtlRowsIH6KgCZpbAZAIq1LUBYGPNrfsY/ez3YwYlyw49xlMQZG7VocC287YSyc7CrGwnNXf6ldrV3eNFHo1Np12nl4x7Hpp1Xn2a06oz3LwQdOtB40ksLE1Jkbz7/R3nPk0UwjSoLSzL5TC29DaW848OTeq1Yn6jRe3Hl12As3m/ZU60kjfZuLao8KW8vafTjY9LKEfuFzkyFtGfQj83pVu1op6rIwnfCzdrLolfTHYki7UWTxIGiG0q5RZuBS7F3T7JOMGtXKlqXKvJfFmrRbnerb6UGp0uh6Vdm0P/BCqZAkQUArLtX88CSj+IklKdu6gZ8kiaK4puv/9gP8E+0th9I7ord+FFKRmsMAAAAASUVORK5CYII=";
const BACKGROUND_RIGHT_URL = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAB4AAAAeCAQAAACROWYpAAABKklEQVQ4y+XUwU7cMBRG4S9OyoDQdFGKQGx4gz4eD9E3rFR1XyEhQCWaZGK7m6EicTIt0xXiLm0d6dj/vbe6qX3xTfvV6ytIfugOIBFw4eRQOFjLh8LJd5u3pt3s0f5gpZZ0+iWYC9GvyfnKubUaJE9utXPwnPbalexRJ6kcOXXtp7t/0T5xqXWvE2WV4NFHn0UPf9MOzrTudNLuJBoMkk9a2/2/fSy4f4FC1nuwdTqNaqq9spmgz/iTI9UYnjZJ0IkzueQysFI7zqLk8qbUHixVZRiHWmr3qvHL/qBhKl5qb6VdZ42rZro0Su1soynwoNFPM5gbyajXqF/I1xpx3CAs9XaU1YIs735gKJK3PJJJ2mFZXpr3fZskS5K0vCre4wL8T+3szPFh8G8+SIOIlEKP2QAAAABJRU5ErkJggg==";

/**
 * Show the output of the printer.
 */
export class Printer {
    private readonly node: HTMLElement;
    private readonly linePrinterPaper: HTMLElement;
    private readonly lines: string[] = [];
    private line = "";

    constructor(parentNode: HTMLElement) {
        addPrinterCssFontToPage();

        parentNode.style.position = "relative";

        this.node = document.createElement("div");
        this.node.style.backgroundColor = "#ddddd8";
        this.node.style.boxSizing = "border-box";
        this.node.style.display = "grid"; // For centering the children.
        this.node.style.position = "absolute";
        this.node.style.inset = "0";

        this.linePrinterPaper = document.createElement("div");
        this.linePrinterPaper.style.width = "100%";
        this.linePrinterPaper.style.height = "100%";
        this.linePrinterPaper.style.padding = "20px 0";
        this.linePrinterPaper.style.fontFamily = `"${PRINTER_REGULAR_FONT_FAMILY}", monospace`;
        this.linePrinterPaper.style.fontSize = "12px"; // To get 80 chars across.
        this.linePrinterPaper.style.lineHeight = "1.5";
        this.linePrinterPaper.style.boxSizing = "border-box";
        this.linePrinterPaper.style.color = "#222";
        this.linePrinterPaper.style.overflowY = "scroll";
        this.linePrinterPaper.style.background = "local url(" + BACKGROUND_LEFT_URL + ") top left repeat-y, " +
            "local url(" + BACKGROUND_RIGHT_URL + ") top right repeat-y, #fffff8";

        this.node.append(this.linePrinterPaper);
        parentNode.append(this.node);
    }

    public printChar(ch: number): void {
        // console.log("Printing \"" + String.fromCodePoint(ch) + "\" (" + ch + ")");
        if (ch === 13) {
            // Carriage return, end of line.
            this.printLine(this.line);
            this.line = "";
        } else if (ch === 10) {
            // Linefeed, a simple "LPRINT" will generate this by itself (no carriage return).
            if (this.line.length === 0) {
                this.printLine("");
            }
        } else {
            this.line += String.fromCodePoint(ch);
        }
    }

    public savePrintout() {
        const a = document.createElement("a");
        const contents = this.lines.join("\n") + "\n";
        const blob = new Blob([contents], {type: "text/plain"});
        a.href = window.URL.createObjectURL(blob);
        a.download = "printout.txt";
        a.click();
    }

    public clearPrintout() {
        this.linePrinterPaper.replaceChildren();
        this.lines.splice(0, this.lines.length);
    }

    private printLine(line: string): void {
        this.lines.push(line);

        // Figure out scroll space at the bottom:
        const bottomSpace = Math.abs(this.linePrinterPaper.scrollHeight - this.linePrinterPaper.scrollTop - this.linePrinterPaper.clientHeight);
        // There's some rounding, be sloppy:
        const wasAtBottom = bottomSpace < 1;

        // Add the new line.
        const lineNode = document.createElement("div");
        lineNode.style.padding = "0 40px";
        lineNode.style.whiteSpace = "pre-wrap";
        lineNode.style.minHeight = "1lh"; // For blank lines.
        lineNode.textContent = line;
        this.linePrinterPaper.append(lineNode);

        if (wasAtBottom) {
            // Stay scrolled at the bottom.
            this.linePrinterPaper.scrollTop = this.linePrinterPaper.scrollHeight;
        }
    }
}
