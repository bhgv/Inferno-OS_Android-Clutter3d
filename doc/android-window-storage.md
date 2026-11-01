# Phone video follow-up: window storage and Activity lifetime

Requires Inferno experiment adf3a30f0c9200d939142d91a677bbba852340c3 or later.
Independent GLES images must retain their own row stride, origin and ownership.
The core regression covers a narrow nonzero-origin window and allocation errors.

The launcher keeps logical actor size stable during texture uploads; clipping
changes only visibility and never resizes the borrowed image buffer. Native
configuration changes retain the Activity. When its running VM Activity really
terminates, exit the process: native VM workers cannot borrow a retired Activity.
No FB coordinate transform is imported into this renderer.

Host input/startup/UI-thread/clip/private-FreeType checks pass. Exact font
appearance, Bounce and repeated launch on the reported phone remain device
checks; the supplied videos alone do not identify a native fault PC. Preserve
/sdcard/Inferno/startup.log and backtrace.log if the new test APK still crashes.
