#pragma once

#include <vector>

namespace tw
{
	class Environment;
	class BaseCharacterModel;

	class AbstractRenderer
	{
	public:
		AbstractRenderer();
		virtual ~AbstractRenderer();

		virtual void render(Environment* environment, std::vector<BaseCharacterModel*> & characters) = 0;
	};
}
