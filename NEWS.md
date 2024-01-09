Inkscape 1.3.2

Released on 2023-11-25

    Fix for a regression introduced in 1.3.1 with path data for shapes

Inkscape 1.3.1

Released on 2023-11-18

    Improvements on node count when using shape builder
    Ability to disable snapping to grid lines
    Possibility to split text into characters while keeping kerning
    Lots of bugfixes and crash fixes, and translation improvements





Inkscape 1.3

Release highlights

Released on 2023-07-21

    Improved performance thanks to fully asynchronous, multithreaded rendering
    Shape builder (NEW) - a new tool for building complex shapes (Boolean tool)
    Improved On-Canvas Pattern Editing
    Pattern Editor (NEW)
    Page margins & bleed
    Document Resources Dialog (NEW)
    Return of Search, opacity & blend modes in Layers & Objects dialog and of an optional persistent snap bar
    Font Collections (NEW)
    Syntax highlighting in XML Editor
    LPE dialog - Redesign
    Better PDF import
    Many crash & bug fixes


==================================================================
===                                                            ===
===     The authoritative version of the changelog is at       ===
=== https://wiki.inkscape.org/wiki/index.php/Release_notes/1.3 ===
===                                                            ===
==================================================================

Performance

    lot of effort has gone into improving the performance and speed of all aspects in Inkscape. This involved the refactoring of inefficient code, rewriting how Inkscape works with patterns, moving bitmap tracing into a separate thread and so much more.
    canvas rendering is now both multithreaded and done outside of Inkscape's main process thread. This should significantly improve performance while zooming / panning / transforming objects
    if your computer's processor has more than one core (which it most likely does). This results in a 2–4× speedup in most of the tasks.
    you can set the number of processor cores Inkscape should use for rendering in Edit ➞ Preferences ➞ Rendering ➞ Number of Threads. By default, Inkscape tries to be as fast as possible by using as many cores as possible

General user interface
Color Palette

    Color palette fields now have little indicators that show which color is used for the stroke and fill of a selected object.
    Color pinning

Color Pickers

    Color pickers in Inkscape now support choosing colors in the OKLch color space, which has just been adopted into the CSS Color Module Level 4 draft recommendation. 
    This additional option is disabled by default. It can be enabled in Edit ➞ Preferences ➞ Interface ➞ Color Selector as "OKHSL", and will then be available in any color picker's dropdown. Note that color values will still be written as RGB hex codes in the SVG source, and Inkscape also does not support reading colors that are defined in that color space. This change is purely adding a new convenient option for choosing colors.

Command Palette

    It is no longer necessary to scroll horizontally in the Command Palette (shortcut: ?), as entries are now nicely arranged and formatted and make use of linebreaks. Now all the info for an entry is directly visible 

Context menu

    For clipped images, there is now an option to crop them to their clipping path. This destructive operation can be used to reduce the file size of an SVG file, removing unneeded parts. The function automatically embeds any linked images, leaving the original image untouched. For any areas outside an irregular-shaped clip, but inside the rectangular region of the bounding box, the new image will use transparency. The status bar will show a message telling you by how many bytes the cropping made your file lighter.

Copy-pasting Styles

    A new preference option in Edit > Preferences > Behavior > Clipboard allows you to choose whether you want to replace the CSS rules for an object with those of the other object, or whether you want to always just paste the resulting style attributes, without any CSS classes, when you copy-paste the style of one object onto another one. This will help with (colorable) icon creation and web development tasks.

Crash dialog

    When Inkscape crashes, it will now ask you to create a bug report and will provide information that can help developers to fix the crash.

Keyboard shortcuts

    The keyboard shortcuts for aligning objects vertically and horizontally have been moved to the numeric keypad, where the other alignment shortcuts are

Origin on current page

    In Edit > Preferences > Interface, there is an option now to use the current page's corner as the coordinate system origin for placing objects, for the rulers, and for any tools.

Pasting Options Renamed, and Paste on Page

    The Paste size entry in the Edit menu has been renamed to Paste…, to hold all 'special' pasting operations that you may need.
    A new pasting feature On Page has been added to this renamed menu to paste the copied object(s) into the same position on a different page

Rulers

    Inkscape's rulers at the canvas boundaries got two new indicator areas:
        Page: the part of the ruler that corresponds to the current page's location is now colored in a different tone, so you can always see where your page ends.
        Selection: a thin blue line indicates and follows the current selection. This line can be turned off in Edit ➞ Preferences ➞ Interface: Show selection in ruler.
    Ruler performance has been improved along with these changes.

Selecting

    Functionality to save and restore the current selection (i.e. which items are currently selected) and to delete the saved status has been added. It is accessible from the Commands bar (?, search for 'set selection backup') or by setting a keyboard shortcut for it in the preferences. You can use it to quickly save which objects or which nodes in a path you currently have selected, and to later get back to work with that selection.
    An option to select invisible (transparent) items by clicking on them has been added to the preferences in Edit > Preferences > Behavior > Selecting.

Snap toolbar

    An option was added in Edit ➞ Preferences ➞ Interface ➞ Toolbars to show snapping options permanently in a dedicated toolbar, similar to Inkscape version 1.1 and earlier.



Canvas
Views and Display Modes

    Quick Preview: Pressing F temporarily hides on-canvas overlays (transformation handles, grids, guides...). This allows quick preview of final artwork without any distractions. https://gitlab.com/inkscape/inkscape/-/merge_requests/4395
    Added display overlay controls in top right corners. You need to have scrollbars enabled to see it (CTRL+B).
    Clip object rendering to page: For a more permanent preview, you can choose whether to not display any objects outside the page area In the Document Properties dialog. A keyboard shortcut to toggle this feature can be set in the Keyboard Shortcuts list in the preferences.

OpenGL (GPU) accelerated canvas

    An OpenGL-accelerated display mode was added to the canvas to speed up panning, zooming and rotating. This is NOT a fully GPU-based renderer; content is still rendered on the CPU in exactly the same way as before, so large performance improvements are not to be expected. It does however result in a smoother display and lower CPU usage, especially on HiDPI screens. OpenGL mode is highly experimental and is turned off by default. It can be turned on at Preferences ➞ Rendering ➞ Enable OpenGL

Smooth auto-scrolling

    Auto-scrolling happens when you drag an object off the edge of the canvas. We improved smoothness of this action.


Tools
General

    You can right click on any tool icon in toolbox to see tool preferences.

3D Box Tool

    This tool had to sacrifice its shortcut x, which is now used for the Shape Builder Tool. The shortcut Shift+F4 still works for making 3D boxes.

Gradient Tool

    Allow changing the repeat setting when multiple gradients are selected.
    Show 'Multiple gradients' in the stop list when multiple gradients are selected (instead of a random stop).
    Allow editing of the offset of the start/end stops in the tool controls (consistent with the option in the Fill and Stroke dialog).
    Keep the stop selected after the offset is changed in the toolbar (instead of selecting the first stop of the gradient).

Node Tool

    On-Canvas Pattern Editing: Pattern editing on canvas is now easier; you can click on any part of a pattern and it will show you controls at that position. We also added an outline that shows you the edges of the pattern. The first square controls position, the circle controls rotation, and the second square controls size. Hold Shift to constrain proportions. We also fixed performance problems with patterns, so now you can have smaller patterns in project and zoom in on them without worrying about Inkscape eating up all your RAM.
    Draw around Selection: We added a new (lasso) selection mode for nodes. Hold Alt and draw with the Node tool around the nodes that you want to select. This saves a lot of time that was needed before, where you needed to add new nodes to the selection by dragging small rectangles while holding Shift, whenever nodes were not located together in a convenient rectangular area (MR #4747).
    Better shape preservation when deleting nodes: New, improved curve fitting algorithm from FontForge used when deleting nodes on a "smooth" path (rather than corners)
    Edit Blur on Canvas: New on-canvas blur controls will appear for blur effects from the Fill and Stroke panel, or filters with blur effects from Add Filter. Controls are not linked by default, so you can control horizontal and vertical blurring separately. If you hold CTRL, you can control both Axes linked. You can control arbitrary angle of blurring if you rotate your object after you set blur.
    On-canvas Corners Editing: In the tool controls bar, a new button allows you to add the Corners LPE to the currently selected path. Click the button again to remove the effect

Page Tool

    The Page tool now has controls for margins and bleed
    An attribute on the page element to record the margin
    A new HTML/CSS style box model with tests
    New UI to set margins in the toolbar
    New on-canvas controls for moving margins (with ctrl/shift)
    New display of margins in the same canvas group as the page border
    Snapping for page margins

Selector Tool

    Reapply transform: Ctrl+Alt+T - This allows a user to perform a transformation multiple times and works from the canvas edits or from transform dialog or the select toolbar. Note: on Linux, the shortcut will usually open a terminal, so you may want to assign a different shortcut.
    Duplicate and transform: Ctrl+Alt+D -This performs a duplication and then reapplies the previous transform to the duplicate. Note: On Linux, this shortcut usually minimizes the window, so you will want to assign a different shortcut.
    Clone while dragging: drag object + C - Drag and object and press C to clone it in the current position. https://gitlab.com/inkscape/inkscape/-/merge_requests/4752

Shape Builder Tool
    
    New tool for fast shape building and Boolean operations. Shortcut: X.
    Use: Select multiple overlapping shapes and select the Shape Builder tool. The selection will be fragmented on overlapping areas, while everything else will be hidden until you leave the shape builder. Now you Click and drag to combine segments together or hold Shift + Click and drag to subtract, and Single click on segments to split. Adding is represented by a blue color, removing by pink.

Path Operations

    Object to Path: Path ➞ Object to Path now behaves differently for texts. In recent Inkscape versions, a text was converted into a group of letters, where each letter was a single path. Now, the whole text is converted to a single path (if you need individual letters, with Path ➞ Split Path, many texts can almost be split into letters again - or you can use the extension Text ➞ Split Text to split the text into single letters first).
    Fracture Paths: Path ➞ Fracture - every overlapping region of a set of paths will be split into a separate object. The resulting objects do not overlap anymore.
    Flatten Paths: Path ➞ Flatten - overlapping objects will be flattened visually (it will delete paths that are hidden behind a top path). Useful for separating colors for Screen printing and offset printing as well as for doing any kind of plotting.

Clones
    
    A new preference option is now available, that allows you to decide whether you really want to convert a clone in the selection to a path when you use the command 'Path > Object to Path'. Otherwise, the clones will only be unlinked, but keep their path effects and editable shapes.

Masking / Clipping

    A new option to preserve clips / masks when ungrouping objects has been added (Edit ➞ Preferences ➞ Behavior ➞ Clippaths and masks: When ungroup, clip/mask is preserved in children). The option is active by default. This means that when you now ungroup a group that has been clipped, the elements inside it will inherit the clip. Previously, the clip was removed and everything became un-clipped. To go back to previous default behavior, deactivate this new option. (MR #3564)

Dialogs
Document Resources Dialog

    Added a new dialog that shows you an overview of what assets are currently inside your document. You can edit names and export some of the resources from this dialog.

Export Dialog

    We added new options for how to export multipage in PDF and SVG formats allowing the selection of a single page out of many to export in the single-export tab and improving how batch exporting is done by ordering pages correctly.

Fill and Stroke Dialog

    Markers: Inkscape's markers got some more human-friendly (and better translatable) names.
    Pattern editor: Added in to UI. You can preview patterns, change Name, Size, Rotation, Offset, Gaps, and Colors for some specific patterns. We also added collections of patterns in ~paint/ so it's easier to be organized. Since this allows having many more patterns preinstalled, we also added a search function and a few new default patterns.


Filter Editor

    Redesign of this dialog

Layers and Objects Dialog

    UX improvements

Live Path Effects Dialog

    The compact new design merges organization and controls into one unit. You can reorder LPEs by dragging and dropping the whole effect. It adds a fast search box, and a fast dropdown for adding effects.

Object Attributes Dialog

    An improved dialog that allows you to set contextual object-dependent attributes for the selected object. It is already used for images, accessible as 'Image properties' from the context menu, "Object attributes" from dialog popup menu, and can also be opened for other objects by setting a keyboard shortcut for it in the preferences.

Swatches Dialog

    In the Swatches dialog, the option to display colors together with their names (from the .gpl palette file), in a vertical list, is back again.

Symbols Dialog

    Redesign and functionality improvements

Text and Font Dialog

    Font collections: New feature that allows you to organize your fonts to collections. You can create your Collection and then drag and drop fonts in to it . For example Favorite fonts that you use often or Collection based on fonts properties Like Scripts, Serif, Sans Serifs , etc.

Trace Bitmap Dialog

    Got significant performance boost and a progress bar. Now it runs in the background, allowing you to cancel it if it's taking too long. https://gitlab.com/inkscape/inkscape/-/merge_requests/4702

Welcome Dialog

    Files are sorted by their last modified date. We Added recovery for files in this list after crash. You can see then by text Emergency save next to file.

XML Editor

    Syntax highlighting in XML Editor
    small cosmetic changes to UI


Import / Export

    Many improvements to PDF import
    Rewrite of XAML export
    Improved HPGL import

SVG Standards Compliance

    Added support for href and xlink:href in SVG header. This makes Inkscape more compatible with SVG 2.

Customization / Theming
    
    Added user.css to UI folder to allow tweaking UI without interfering with or fully overriding other CSS files. https://gitlab.com/inkscape/inkscape/-/merge_requests/5004

Multiuser resource sharing
    
    In Edit > Preferences > System, users can set a folder for shared default resources. It must be structured like a user's Inkscape preferences directory. This makes it possible to share a set of resources, such as extensions, fonts, icon sets, keyboard shortcuts, patterns/hatches, palettes, symbols, templates, themes and user interface definition files, between multiple users who have access to that folder (on the same computer or in the network). The option requires a restart of Inkscape to work when changed.


Other

    Many bugfixes
