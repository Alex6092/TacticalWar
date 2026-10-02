#pragma once
#include <MoveActionAnimationEventListener.h>
#include "BattleActionToAnimation.h"
#include "IScreenActionCallback.h"
#include <SFML/Audio.hpp>

class LaunchSpellAction : public BattleActionToAnimation, MoveActionAnimationEventListener
{
private:
	int persoId;
	int spellId;
	int x, y;
	bool moveAnimationFinished;
	IScreenActionCallback * screen;
	bool firstUpdate;
	std::vector<tw::AttackDamageResult> impactedEntities;

	SpellView * spellView;
	sf::SoundBuffer spellSoundBuffer;
	sf::Sound spellSound;

	bool hasResetAttackAnimation;
	bool hasResetTakeDamageAnimation;
	bool animationFinished;
public:
	LaunchSpellAction(IScreenActionCallback * screen, int persoId, int spellId, int x, int y)
	{
		this->persoId = persoId;
		this->spellId = spellId;
		this->screen = screen;
		this->x = x;
		this->y = y;
		moveAnimationFinished = false;
		firstUpdate = true;
		hasResetAttackAnimation = false;
		hasResetTakeDamageAnimation = false;
		animationFinished = false;

		spellView = NULL;
		std::string animPath = screen->getCharacter(persoId)->getSpellAnimationPath(spellId);
		if (animPath.size() > 0)
		{
			spellView = new SpellView(x, y);
			spellView->loadAnimation(animPath);
		}

		std::string animSoundPath = screen->getCharacter(persoId)->getSpellSoundPath(spellId);
		if (animSoundPath.size() > 0)
		{
			spellSoundBuffer.loadFromFile(animSoundPath);
			spellSound.setBuffer(spellSoundBuffer);
		}
	}

	~LaunchSpellAction()
	{
		if(spellView != NULL)
			delete spellView;

		spellSound.stop();
	}

	virtual void update(float deltatime)
	{
		elapseTime(deltatime);

		tw::BaseCharacterModel * spellLauncher = screen->getCharacter(persoId);

		if (firstUpdate)
		{
			impactedEntities = spellLauncher->doAttack(spellId, x, y);
			spellLauncher->setOrientationToLookAt(x, y);

			tw::Animation attackerAnim = spellLauncher->getSpellAttackerAnimation(spellId);
			if (attackerAnim == tw::Animation::ATTACK1)
			{
				spellLauncher->startAttack1Animation(1);
			}
			else if (attackerAnim == tw::Animation::ATTACK2)
			{
				spellLauncher->startAttack2Animation(1);
			}

			spellSound.play();

			screen->applyCharacterLaunchSpell(persoId, x, y, spellId);	// Notifie le screen pour affichage dans les événements de combat ...
			firstUpdate = false;
		}

		if (getEllapsedTime() >= 250 && getEllapsedTime() <= 750)
		{
			if (spellView != NULL)
			{
				spellView->update(deltatime / 1000.0);
				screen->addAnimationToDisplay(spellView);
			}
		}

		if (getEllapsedTime() > 1000 && !hasResetAttackAnimation)
		{
			spellLauncher->resetAnimation();
			hasResetAttackAnimation = true;

			if (impactedEntities.size() > 0)
			{
				for (int i = 0; i < impactedEntities.size(); i++)
				{
					tw::BaseCharacterModel * target = impactedEntities[i].getCharacter();
					int damage = impactedEntities[i].getDamage();
					if (damage > 0)
					{
						screen->playTakeDamageSound();
						if (target->getCurrentLife() <= damage)
						{
							target->startDieAction(1);
						}
						else
						{
							target->startTakeDmg(1);
						}
					}
				}
			}
			else
			{
				animationFinished = true;
			}
		}

		if (!animationFinished && getEllapsedTime() > 2000 && !hasResetTakeDamageAnimation)
		{
			animationFinished = true;
		}

		if (animationFinished)
		{
			for (int i = 0; i < impactedEntities.size(); i++)
			{
				impactedEntities[i].getCharacter()->resetAnimation();
				impactedEntities[i].getCharacter()->modifyCurrentLife(-impactedEntities[i].getDamage());	// Applique les dégats ...
			}
			notifyAnimationFinished(0);
			delete this;
		}
	}

	virtual void onMoveFinished()
	{
		moveAnimationFinished = true;
		notifyAnimationFinished(0);
	}
};