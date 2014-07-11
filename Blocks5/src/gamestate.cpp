#include "pch.h"
#include "gamestate.h"
#include "engine.h"

GameState::GameState(const std::string& name) : gui(GUI::inst())
{
	this->name = name;

	Engine::inst().registerGameState(this);
}

GameState::~GameState()
{
}

void GameState::onRender()
{
}

void GameState::onUpdate()
{
}

void GameState::onEnter(const ParameterBlock& context)
{
}

void GameState::onLeave(const ParameterBlock& context)
{
}

void GameState::onGetFocus()
{
}

void GameState::onLoseFocus()
{
}

void GameState::onAppGetFocus()
{
}

void GameState::onAppLoseFocus()
{
}

const std::string& GameState::getName() const
{
	return name;
}