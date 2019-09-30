#!/usr/bin/python3
#
# This file contains a copy of the strings of the layout files (.onboard files)
# that needs translation. As the current build system cannot extract the
# strings that need translation directly from the layout files, it gets them
# from this file.
#
# Please, keep the strings alphabetically sorted; it will be easier to check
# whether a given string is already present.


from gettext import gettext as _

TRANSLATABLE_LAYOUT_STRINGS = [
    _("Activate Hover Click"),
    _("Alphanumeric keys"),
    # translators: very short label of the left Alt key
    _("Alt"),
    # translators: very short label of the Alt Gr key
    _("Alt Gr"),
    # translators: very short label of the CAPS LOCK key
    _("CAPS"),
    # translators: very short label of the Ctrl key
    _("Ctrl"),
    # translators: very short label of the DELETE key
    _("Del"),
    _("Double click"),
    _("Drag click"),
    # translators: very short label of the numpad END key
    _("End"),
    # translators: very short label of the numpad ENTER key
    _("Ent"),
    # translators: very short label of the ESCAPE key
    _("Esc"),
    _("Function keys"),
    _("Number block and function keys"),
    _("Hide Onboard"),
    _("Pause learning"),
    # translators: very short label of the HOME key
    _("Hm"),
    # translators: very short label of the INSERT key
    _("Ins"),
    _("Main keyboard"),
    # translators: very short label of the Menu key
    _("Menu"),
    _("Middle click"),
    _("Move Onboard"),
    # translators: very short label of the NUMLOCK key
    _("Nm&#10;Lk"),
    _("Number block and snippets"),
    # translators: very short label of the PAUSE key
    _("Pause"),
    # translators: very short label of the PAGE DOWN key
    _("Pg&#10;Dn"),
    # translators: very short label of the PAGE UP key
    _("Pg&#10;Up"),
    # translators: very short label of the Preferences button
    _("Preferences"),
    # translators: very short label of the PRINT key
    _("Prnt"),
    # translators: very short label of the Quit button
    _("Quit"),
    # translators: very short label of the RETURN key
    _("Return"),
    _("Right click"),
    # translators: very short label of the SCROLL key
    _("Scroll"),
    _("Snippets"),
    _("Space"),
    _("Toggle click helpers"),
    # translators: very short label of the TAB key
    _("Tab"),
    # translators: very short label of the default SUPER key
    _("Win"),
    # translators: description of unicode character U+2602
    _("Umbrella"),
    # translators: description of unicode character U+2606
    _("White star"),
    # translators: description of unicode character U+260F
    _("White telephone"),
    # translators: description of unicode character U+2615
    _("Hot beverage"),
    # translators: description of unicode character U+2620
    _("Skull and crossbones"),
    # translators: description of unicode character U+2622
    _("Radioactive sign"),
    # translators: description of unicode character U+262E
    _("Peace symbol"),
    # translators: description of unicode character U+262F
    _("Yin yang"),
    # translators: description of unicode character U+2639
    _("Frowning face"),
    # translators: description of unicode character U+263A
    _("Smiling face"),
    # translators: description of unicode character U+263C
    _("White sun with rays"),
    # translators: description of unicode character U+263E
    _("Last quarter moon"),
    # translators: description of unicode character U+2640
    _("Female sign"),
    # translators: description of unicode character U+2642
    _("Male sign"),
    # translators: description of unicode character U+2661
    _("White heart suit"),
    # translators: description of unicode character U+2662
    _("White diamond suit"),
    # translators: description of unicode character U+2664
    _("White spade suit"),
    # translators: description of unicode character U+2667
    _("White club suit"),
    # translators: description of unicode character U+266B
    _("Beamed eighth note"),
    # translators: description of unicode character U+2709
    _("Envelope"),
    # translators: description of unicode character U+270C
    _("Victory hand"),
    # translators: description of unicode character U+270D
    _("Writing hand"),
    # translators: description of unicode character U+1F604
    _("Smiling face with open mouth and smiling eyes"),
    # translators: description of unicode character U+1F607
    _("Smiling face with halo"),
    # translators: description of unicode character U+1F608
    _("Smiling face with horns"),
    # translators: description of unicode character U+1F609
    _("Winking face"),
    # translators: description of unicode character U+1F60A
    _("Smiling face with smiling eyes"),
    # translators: description of unicode character U+1F60B
    _("Face savouring delicious food"),
    # translators: description of unicode character U+1F60D
    _("Smiling face with heart-shaped eyes"),
    # translators: description of unicode character U+1F60E
    _("Smiling face with sunglasses"),
    # translators: description of unicode character U+1F60F
    _("Smirking face"),
    # translators: description of unicode character U+1F610
    _("Neutral face"),
    # translators: description of unicode character U+1F612
    _("Unamused face"),
    # translators: description of unicode character U+1F616
    _("Confounded face"),
    # translators: description of unicode character U+1F618
    _("Face throwing a kiss"),
    # translators: description of unicode character U+1F61A
    _("Kissing face with closed eyes"),
    # translators: description of unicode character U+1F61C
    _("Face with stuck-out tongue and winking eye"),
    # translators: description of unicode character U+1F61D
    _("Face with stuck-out tongue and tightly closed eyes"),
    # translators: description of unicode character U+1F61E
    _("Disappointed face"),
    # translators: description of unicode character U+1F620
    _("Angry face"),
    # translators: description of unicode character U+1F621
    _("Pouting face"),
    # translators: description of unicode character U+1F622
    _("Crying face"),
    # translators: description of unicode character U+1F623
    _("Persevering face"),
    # translators: description of unicode character U+1F629
    _("Weary face"),
    # translators: description of unicode character U+1F62B
    _("Tired face"),
    # translators: description of unicode character U+1F632
    _("Astonished face"),
    # translators: description of unicode character U+1F633
    _("Flushed face"),
    # translators: description of unicode character U+1F635
    _("Dizzy face"),

    # translators: this is a 'tooltip' of the key_template 'hoverclick' in keyboard layout 'key_defs.xml'
    _("Activate Hover Click"),

    # translators: this is a 'tooltip' of the key 'DC02' in keyboard layout 'Small.onboard'
    _("Angry face"),

    # translators: this is a 'tooltip' of the key_template 'layer4' in keyboard layout 'Whiteboard.onboard'
    _("Arrows"),

    # translators: this is a 'tooltip' of the key 'DC04' in keyboard layout 'Small.onboard'
    _("Astonished face"),

    # translators: this is a 'tooltip' of the key 'DB02' in keyboard layout 'Small.onboard'
    _("Beamed eighth note"),

    # translators: this is a 'tooltip' of the key 'DC05' in keyboard layout 'Small.onboard'
    _("Confounded face"),

    # translators: this is a 'tooltip' of the key 'DC03' in keyboard layout 'Small.onboard'
    _("Crying face"),

    # translators: this is a 'tooltip' of the key_template 'layer5' in keyboard layout 'Whiteboard.onboard'
    _("Custom"),

    # translators: this is a 'summary' of the keyboard layout 'Full Keyboard.onboard'
    _("Desktop keyboard with edit and function keys"),

    # translators: this is a 'tooltip' of the key_template 'doubleclick' in keyboard layout 'key_defs.xml'
    _("Double click"),

    # translators: this is a 'tooltip' of the key_template 'dragclick' in keyboard layout 'key_defs.xml'
    _("Drag click"),

    # translators: this is a 'tooltip' of the key 'DB04' in keyboard layout 'Small.onboard'
    _("Envelope"),

    # translators: this is a 'tooltip' of the key 'DC07' in keyboard layout 'Small.onboard'
    _("Face savouring delicious food"),

    # translators: this is a 'tooltip' of the key 'DD05' in keyboard layout 'Small.onboard'
    _("Face with stuck-out tongue and winking eye"),

    # translators: this is a 'tooltip' of the key 'DD09' in keyboard layout 'Small.onboard'
    _("Female sign"),

    # translators: this is a 'tooltip' of the key 'DC01' in keyboard layout 'Small.onboard'
    _("Frowning face"),

    # translators: this is a 'tooltip' of the key_template 'layer0' in keyboard layout 'Whiteboard.onboard'
    _("Greek"),

    # translators: this is a 'summary' of the keyboard layout 'Grid.onboard'
    _("Grid of keys, suitable for keyboard scanning"),

    # translators: this is a 'tooltip' of the key_template 'hide' in keyboard layout 'key_defs.xml'
    _("Hide Onboard"),

    # translators: this is a 'tooltip' of the key 'hide' in keyboard layout 'Whiteboard_wide.onboard'
    _("Hide keyboard"),

    # translators: this is a 'description' of the keyboard layout 'Whiteboard.onboard'
    _("Keyboard layout with Greek literals, arrows and more, for math and physics education on an interactive Whiteboard."),

    # translators: this is a 'description' of the keyboard layout 'Whiteboard_wide.onboard'
    _("Keyboard layout with Greek literals, arrows and more, for math and physics education on an interactive Whiteboard.\nThis layout places all keys on a single, very wide layer."),

    # translators: this is a 'tooltip' of the key 'DD06' in keyboard layout 'Small.onboard'
    _("Kissing face with closed eyes"),

    # translators: this is a 'tooltip' of the key 'DC10' in keyboard layout 'Small.onboard'
    _("Last quarter moon"),

    # translators: this is a 'tooltip' of the key_template 'layer0' in keyboard layout 'key_defs.xml'
    _("Main keyboard"),

    # translators: this is a 'tooltip' of the key 'DC09' in keyboard layout 'Small.onboard'
    _("Male sign"),

    # translators: this is a 'summary' of the keyboard layout 'Compact.onboard'
    _("Medium size desktop keyboard"),

    # translators: this is a 'tooltip' of the key_template 'middleclick' in keyboard layout 'key_defs.xml'
    _("Middle click"),

    # translators: this is a 'summary' of the keyboard layout 'Phone.onboard'
    _("Mobile keyboard for small screens"),

    # translators: this is a 'tooltip' of the key 'expand-corrections' in keyboard layout 'word_suggestions.xml'
    _("More suggestions"),

    # translators: this is a 'tooltip' of the key_template 'move' in keyboard layout 'key_defs.xml'
    _("Move Onboard"),

    # translators: this is a 'tooltip' of the key 'move' in keyboard layout 'Whiteboard_wide.onboard'
    _("Move keyboard"),

    # translators: this is a 'tooltip' of the key_template 'layer1' in keyboard layout 'key_defs.xml'
    _("Number block and snippets"),

    # translators: this is a 'tooltip' of the key 'pause-learning.wordlist' in keyboard layout 'word_suggestions.xml'
    _("Pause learning"),

    # translators: this is a 'tooltip' of the key 'DB06' in keyboard layout 'Small.onboard'
    _("Peace symbol"),

    # translators: this is a 'tooltip' of the key_template 'settings' in keyboard layout 'key_defs.xml'
    _("Preferences"),

    # translators: this is a 'tooltip' of the key 'quit' in keyboard layout 'Whiteboard_wide.onboard'
    _("Quit Keyboard"),

    # translators: this is a 'tooltip' of the key_template 'quit' in keyboard layout 'key_defs.xml'
    _("Quit Onboard"),

    # translators: this is a 'tooltip' of the key 'DB09' in keyboard layout 'Small.onboard'
    _("Radioactive sign"),

    # translators: this is a 'tooltip' of the key_template 'secondaryclick' in keyboard layout 'key_defs.xml'
    _("Right click"),

    # translators: this is a 'tooltip' of the key_template 'layer2' in keyboard layout 'Whiteboard.onboard'
    _("Set and Settings"),

    # translators: this is a 'tooltip' of the key 'settings' in keyboard layout 'Whiteboard_wide.onboard'
    _("Settings"),

    # translators: this is a 'tooltip' of the key 'DD01' in keyboard layout 'Small.onboard'
    _("Smiling face"),

    # translators: this is a 'tooltip' of the key 'DD08' in keyboard layout 'Small.onboard'
    _("Smiling face with halo"),

    # translators: this is a 'tooltip' of the key 'DC08' in keyboard layout 'Small.onboard'
    _("Smiling face with horns"),

    # translators: this is a 'tooltip' of the key 'DD03' in keyboard layout 'Small.onboard'
    _("Smiling face with open mouth and smiling eyes"),

    # translators: this is a 'tooltip' of the key 'DD02' in keyboard layout 'Small.onboard'
    _("Smiling face with smiling eyes"),

    # translators: this is a 'tooltip' of the key 'DD07' in keyboard layout 'Small.onboard'
    _("Smiling face with sunglasses"),

    # translators: this is a 'tooltip' of the key_template 'layer2' in keyboard layout 'key_defs.xml'
    _("Snippets"),

    # translators: this is a 'summary' of the keyboard layout 'Small.onboard'
    _("Space efficient desktop keyboard"),

    # translators: this is a 'tooltip' of the key_template 'layer3' in keyboard layout 'Whiteboard.onboard'
    _("Special Characters"),

    # translators: this is a 'summary' of the keyboard layout 'Whiteboard_wide.onboard'
    _("Special characters for interactive Whiteboards, wide"),

    # translators: this is a 'summary' of the keyboard layout 'Whiteboard.onboard'
    _("Special characters for interactive whiteboards"),

    # translators: this is a 'tooltip' of the key 'correction' in keyboard layout 'word_suggestions.xml'
    _("Spelling suggestion"),

    # translators: this is a 'tooltip' of the key_template 'layer1' in keyboard layout 'Whiteboard.onboard'
    _("Sub-, Superscripts; Fractions"),

    # translators: this is a 'tooltip' of the key 'DC06' in keyboard layout 'Small.onboard'
    _("Tired face"),

    # translators: this is a 'tooltip' of the key_template 'showclick' in keyboard layout 'key_defs.xml'
    _("Toggle click helpers"),

    # translators: this is a 'tooltip' of the key 'DB05' in keyboard layout 'Small.onboard'
    _("Umbrella"),

    # translators: this is a 'tooltip' of the key 'DB01' in keyboard layout 'Small.onboard'
    _("White heart suit"),

    # translators: this is a 'tooltip' of the key 'DB07' in keyboard layout 'Small.onboard'
    _("White star"),

    # translators: this is a 'tooltip' of the key 'DD10' in keyboard layout 'Small.onboard'
    _("White sun with rays"),

    # translators: this is a 'tooltip' of the key 'DB03' in keyboard layout 'Small.onboard'
    _("White telephone"),

    # translators: this is a 'tooltip' of the key 'DD04' in keyboard layout 'Small.onboard'
    _("Winking face"),

    # translators: this is a 'tooltip' of the key 'DB08' in keyboard layout 'Small.onboard'
    _("Yin yang"),
]

raise Exception("This module should not be executed.  It should only be"
        " parsed by i18n tools such as intltool.")
