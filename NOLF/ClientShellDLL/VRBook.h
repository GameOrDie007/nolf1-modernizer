// VRBook.h: the Game Manual row. The game's manual as a book in the player's
// hand, drawn by the Game Tome OpenXR layer the VR host loads.
//
// THE WHOLE INTERFACE is a block of shared memory the layer makes when the
// session starts, named for the host's process id. The game writes where the
// book is and which hand turns its pages, then asks for it; the layer opens it,
// shows it over the paused game and hands the controllers back when it closes.

#ifndef _VRBOOK_H_
#define _VRBOOK_H_

namespace VRBook
{
	// The book ships beside the game folder (..\GameTome\tome.txt).
	bool	Installed();

	// The layer is running and can draw it.
	bool	Ready();

	// Ask the layer to open the book. False if it cannot.
	bool	Open();
}

#endif
