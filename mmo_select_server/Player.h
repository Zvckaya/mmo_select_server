#pragma once

class Player
{
public:
	Player(int x,int y);
	Player();
	~Player();

private:
	int _x;
	int _y;
	int _hp;
};

Player::Player():_x(0),_y(0),_hp(0)
{
	
}

Player::Player(int x,int y):_x(x),_y(y),_hp(100)
{
	
}

Player::~Player()
{
}