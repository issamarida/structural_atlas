# Structural Atlas

3D structural atlas for 20 of the most used steroid compounds in bodybuilding.
Built in plain C with SDL2.

## Steroid Collection

Testosterone - Base compound
Testosterone Enanthate - Long ester chain
Testosterone Cypionate - Long ester
Trenbolone - Double bonds
Nandrolone/Deca - 19-nor
Boldenone/EQ - Veterinary
Dianabol - C17 methyl
Anavar - Oxandrolone
Winstrol - Nitrogen hetero
Anadrol - Strong oral
Masteron - DHT derivative
Primobolan - Mild injectable
Turinabol - Chlorinated
Halotestin - Fluorinated
Proviron - Oral DHT
Mibolerone - Cheque Drops
Superdrol - Methylated
Oral Turinabol - Chloro compound
Test Propionate - Short ester
NPP - Short nandrolone

## Features

- Two render modes:
  - Wireframe
  - Ball-and-stick
- Mouse controls:
  - Left drag: rotate
  - Right drag: pan
  - Wheel: zoom
- View:
  - R: reset view
  - A: toggle auto-rotation

Note: The molecular geometry is stylized for education and visualization.

## Requirements

- CMake 3.16+
- A C compiler (gcc or clang)
- SDL2 development package
- pkg-config

On Ubuntu:
```bash
sudo apt update
sudo apt install -y build-essential cmake pkg-config libsdl2-dev
