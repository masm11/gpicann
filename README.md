# Screenshot Annotation Tool

<img src="screenshot.png">

## Requirements

- Gtk+3
- Cairo
- Pango

## Installation

```sh
cd src
make
sudo cp gpicann /usr/bin/
cd ../
sudo cp gpicann.desktop /usr/share/applications/
```

## Usage

1. take a screenshot with another tool.

2. invoke gpicann on a command line:
   ```
   gpicann screenshot.png
   ```

## How to

- Modify text

  Click select item on the toolbar, and double-click your text.

- Raise

  Click select item on the toolbar, click your item, and press `ctrl+f`.

- Lower

  Click select item on the toolbar, click your item, and press `ctrl+b`.

- Undo

  Press `ctrl-z`.

- Redo

  Press `ctrl-shift-z`.

- Delete

  Click select item on the toolbar, click your item, and press `backspace`.

## Author

masm11
