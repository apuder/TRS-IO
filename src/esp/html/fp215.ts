
export type PenColor = [number, number, number];

export const BLACK_INK_COLOR: PenColor = [0, 0, 0];
export const RED_INK_COLOR: PenColor = [200, 0, 0];
export const BLUE_INK_COLOR: PenColor = [0, 0, 255];
export const GREEN_INK_COLOR: PenColor = [0, 150, 0];

/**
 * Parse a numeric parameter, like "5". May contain spaces before
 * and after, but nothing else. Return NaN if the number cannot be parsed.
 */
function parseNumericParameter(arg: string): number {
    let i = 0;

    // Skip initial space.
    while (i < arg.length && arg[i] === " ") {
        i += 1;
    }

    // Parse integer. Don't use parseInt(), it's too permissive and
    // we want to reject strings that the plotter would reject.
    let value = 0;
    while (i < arg.length) {
        const ch = arg.charCodeAt(i);
        if (ch >= 0x30 && ch <= 0x39) {
            value = value*10 + ch - 0x30;
        } else {
            break;
        }
        i += 1;
    }

    // Skip trailing space.
    while (i < arg.length && arg[i] === " ") {
        i += 1;
    }

    return i === arg.length ? value : NaN;
}

/**
 * Emulator for the FP-215 plotter. Takes the ASCII commands and draws to an HTML canvas.
 */
export class Fp215 {
    private readonly canvas: HTMLCanvasElement;
    private readonly ctx: CanvasRenderingContext2D;
    // Accumulated command so far.
    private currentCommand = "";
    // Current pen position, in point space.
    private x = 0;
    private y = 0;
    // Origin of point space.
    private xOrigin = 0;
    private yOrigin = 0;
    // Length of dashes (and spaces) for dashed lines, in points.
    private dashLength = 30;
    // Size of paper.
    private plottingArea: 0 | 1 = 0;
    // 0 = solid, 1 = dashed.
    private lineType: 0 | 1 = 0;
    private penColor: PenColor = [0, 0, 0];

    constructor(canvas: HTMLCanvasElement) {
        this.canvas = canvas;
        this.ctx = canvas.getContext("2d") as CanvasRenderingContext2D;
        this.configureContext();
        this.updatePlottingArea();
    }

    /**
     * Set the pen color as an RGB triple, each component from 0 to 255. The default
     * is black [0, 0, 0].
     */
    public setPenColor(penColor: PenColor): void {
        this.penColor = penColor;
        this.configureContext();
    }

    // Blank out the canvas.
    public newPaper() {
        const oldGlobalCompositeOperation = this.ctx.globalCompositeOperation;
        this.ctx.globalCompositeOperation = "copy";
        this.ctx.fillStyle = "#fffff8";
        this.ctx.fillRect(0, 0, this.canvas.width, this.canvas.height);
        this.ctx.globalCompositeOperation = oldGlobalCompositeOperation;
    }

    public savePlot() {
        const a = document.createElement("a");
        this.canvas.toBlob(blob => {
            if (blob !== null) {
                a.href = window.URL.createObjectURL(blob);
                a.download = "plot.png";
                a.click();
            }
        }, "image/png");
    }

    // Process bytes sent to the plotter. May contain partial commands or more than one
    // command, but each command must be terminated with a newline or linefeed.
    public processBytes(s: string): void {
        for (const ch of s) {
            this.processByte(ch);
        }
    }

    // Process a single byte of a command.
    public processByte(ch: string): void {
        if (ch === "\n" || ch === "\r") {
            this.processCommand(this.currentCommand);
            this.currentCommand = "";
        } else {
            this.currentCommand += ch;
        }
    }

    // Process a single command. Must not contain the terminating newline or linefeed.
    private processCommand(command: string): void {
        if (command.length === 0) {
            return;
        }

        // Break off the arguments and parse them as numbers.
        const rest = command.substring(1);
        const args = rest.split(",");
        const numericArgs = args.map(parseNumericParameter);
        const anyNaNs = numericArgs.some(arg => isNaN(arg));

        switch (command[0]) {
            // Set the length of the dash (and space) for dashed lines.
            case "B": {
                if (numericArgs.length !== 1 || anyNaNs) {
                    console.log("FP-215: Invalid dash length command: " + command);
                } else {
                    this.dashLength = numericArgs[0];
                }
                break;
            }

            // Draw from the current point to the specified point, then to the
            // other points if specified.
            case "D": {
                if (numericArgs.length < 2 || numericArgs.length % 2 !== 0 || anyNaNs) {
                    console.log("FP-215: Invalid drawing command: " + command);
                } else {
                    this.ctx.beginPath();
                    this.ctx.moveTo(this.xToCanvas(this.x), this.yToCanvas(this.y));
                    for (let i = 0; i < numericArgs.length; i += 2) {
                        this.ctx.lineTo(this.xToCanvas(numericArgs[i]), this.yToCanvas(numericArgs[i + 1]));
                    }
                    this.ctx.stroke();
                    this.x = numericArgs[numericArgs.length - 2];
                    this.y = numericArgs[numericArgs.length - 1];
                }
                break;
            }

            // Set the size of the paper.
            case "F": {
                if (numericArgs.length !== 1 || (numericArgs[0] !== 0 && numericArgs[0] !== 1)) {
                    console.log("FP-215: Invalid plotting area command: " + command);
                } else {
                    this.plottingArea = numericArgs[0];
                    this.updatePlottingArea();
                }
                break;
            }

            // Move the pen to the origin.
            case "H": {
                this.x = 0;
                this.y = 0;
                break;
            }

            // Set the origin, either to an explicit location (if specified), or the current
            // pen location if not.
            case "I": {
                if (numericArgs.length === 0) {
                    this.xOrigin = this.x;
                    this.yOrigin = this.y;
                } else if (numericArgs.length !== 2 || anyNaNs) {
                    console.log("FP-215: Invalid set-origin command: " + command);
                } else {
                    this.xOrigin = numericArgs[0];
                    this.yOrigin = numericArgs[1];
                }
                break;
            }

            // Like "D", but the numbers are relative to the previous position.
            case "J": {
                if (numericArgs.length < 2 || numericArgs.length % 2 !== 0 || anyNaNs) {
                    console.log("FP-215: Invalid drawing command: " + command);
                } else {
                    this.ctx.beginPath();
                    this.ctx.moveTo(this.xToCanvas(this.x), this.yToCanvas(this.y));
                    for (let i = 0; i < numericArgs.length; i += 2) {
                        this.x += numericArgs[i];
                        this.y += numericArgs[i + 1];
                        this.ctx.lineTo(this.xToCanvas(this.x), this.yToCanvas(this.y));
                    }
                    this.ctx.stroke();
                }
                break;
            }

            // Set whether to draw solid or dashed lines.
            case "L": {
                if (numericArgs.length !== 1 || (numericArgs[0] !== 0 && numericArgs[0] !== 1)) {
                    console.log("FP-215: Invalid line type command: " + command);
                } else {
                    this.lineType = numericArgs[0];
                    if (this.lineType === 1) {
                        console.log("FP-215: Dashed lines are not supported");
                    }
                }
                break;
            }

            // Move the pen (without drawing) to the specified location.
            case "M": {
                if (numericArgs.length !== 2 || anyNaNs) {
                    console.log("FP-215: Invalid move command: " + command);
                } else {
                    this.x = numericArgs[0];
                    this.y = numericArgs[1];
                }
                break;
            }

            // Draw an icon.
            case "N": {
                console.log("FP-215: Drawing an icon is not supported: " + command);
                break;
            }

            // Draw text.
            case "P": {
                console.log("FP-215: Drawing text is not supported:" + command);
                break;
            }

            // Set text orientation.
            case "Q": {
                console.log("FP-215: Setting text orientation is not supported:" + command);
                break;
            }

            // Like "M" but relative to current location.
            case "R": {
                if (numericArgs.length !== 2 || anyNaNs) {
                    console.log("FP-215: Invalid relative move command: " + command);
                } else {
                    this.x += numericArgs[0];
                    this.y += numericArgs[1];
                }
                break;
            }

            // Set size of text and icons.
            case "S": {
                console.log("FP-215: Setting size of text is not supported:" + command);
                break;
            }

            // Draw a set of axes.
            case "X": {
                console.log("FP-215: Drawing axes is not supported:" + command);
                break;
            }

            default:
                console.log("FP-215: Unknown FP-215 command: " + command);
                break;
        }
    }

    // After setting the plotting area, updates the canvas size to match and clears the paper.
    private updatePlottingArea(): void {
        const widthMm = this.plottingArea === 0 ? 270 : 298;
        const heightMm = this.plottingArea === 0 ? 186 : 216;
        this.canvas.width = widthMm*10;
        this.canvas.height = heightMm*10;
        this.configureContext();
        this.newPaper();
    }

    // Configure the Canvas context for plotting.
    private configureContext(): void {
        const c = this.penColor;
        this.ctx.strokeStyle = `rgb(${c[0]} ${c[1]} ${c[2]} / 50%)`;
        this.ctx.globalCompositeOperation = "multiply";
        this.ctx.lineWidth = 5;
        this.ctx.lineCap = "round";
    }

    // Convert an X coordinate in points to the X coordinate on the canvas.
    private xToCanvas(x: number): number {
        return this.xOrigin + x;
    }

    // Convert a Y coordinate in points to the Y coordinate on the canvas.
    private yToCanvas(y: number): number {
        return this.canvas.height - (this.yOrigin + y);
    }
}
