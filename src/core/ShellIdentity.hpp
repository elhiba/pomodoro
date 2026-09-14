#ifndef SHELL_IDENTITY_HPP
#define SHELL_IDENTITY_HPP

// Gives the process an identity the Windows shell can look up.
//
// Windows names an app in the media flyout, the volume mixer and the notification centre
// by resolving its AppUserModelID, and for an app that was never installed through a
// package there is nothing to resolve unless a shortcut somewhere carries the same id.
// Without one the flyout simply says "Unknown app", whatever the executable's version
// resource claims.
//
// So this sets the process id and makes sure a Start menu entry carrying it exists. The
// shortcut is written once, only when it is missing, and is an ordinary .lnk the user is
// free to delete -- at the cost of the name going back to "Unknown app".
//
// Defined only in the Windows build; nothing else calls it.
void	registerShellIdentity();

#endif
