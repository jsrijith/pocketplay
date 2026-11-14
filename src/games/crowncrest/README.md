# Crown & Crest

The four dice for Crown & Crest. Each die has its own six faces: shields with stripes, dots or quarters, some with X marks beside them, and a `?` on dice 3 and 4.

## Getting around

- `ROLL`, or the Down button, rolls all four dice together. Each die shows the face it landed on, and the label under it says which die it is.
- Before the first roll the dice are blank and grey.
- Roll as often as you like: there's no score or round count yet.
- `BACK` in the header returns to the home page.

## How the dice stay fair

Every roll mixes the chip's random number with the exact microsecond of your tap, then picks each face by rejection sampling, so all six faces of every die are exactly equally likely. The PC tests roll 600,000 times and check each die's faces, and that the dice don't follow each other.

## Saves

All in `/.pocketplay/`:

| Name | Holds |
|---|---|
| `crowncrest` | The last roll, so sleep or a flat battery brings back the same dice. |

The icon is Lucide's shield (ISC licence).
