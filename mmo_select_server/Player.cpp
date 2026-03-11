#include "Player.h"

Player::Player()
	: _x(0), _y(0), _hp(100), _direction(0), _moveDir(0), _action(PLAYER_ACTION_NONE)
{
}

Player::Player(short x, short y, unsigned char direction)
	: _x(x), _y(y), _hp(100), _direction(direction), _moveDir(direction), _action(PLAYER_ACTION_NONE)
{
}

Player::~Player()
{
}
