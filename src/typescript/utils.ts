
export const CSS_PREFIX = "trs-io-";

// Convert RGB array (0-255) to a CSS string.
export function rgbToCss(color: number[]): string {
    return "#" + color.map(c => c.toString(16).padStart(2, "0").toUpperCase()).join("");
}

// Multiplies an RGB (0-255) color by a factor.
export function adjustColor(color: number[], factor: number): number[] {
    return color.map(c => Math.max(0, Math.min(255, Math.round(c*factor))));
}
