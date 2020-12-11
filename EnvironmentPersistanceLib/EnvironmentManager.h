#pragma once

#include <Environment.h>
#include <stdio.h>
#include <filesystem>
#include <vector>
#include <iostream>
#include <exception>

namespace fs = std::filesystem;

namespace tw
{
	class EnvironmentManager
	{
	private:
		static Environment * testEnvironment;
		void createTestEnvironmentIfNotExists();


		static EnvironmentManager * instance;
		EnvironmentManager()
		{

		}

	public:
		static EnvironmentManager * getInstance();

		Environment * loadEnvironment(int environmentId);
		void saveEnvironment(Environment * environment);

		Environment * getRandomEnvironment()
		{
			//createTestEnvironmentIfNotExists();
			std::vector<int> ids = getAlreadyExistingIds();
			if (ids.size() > 0)
			{
				return loadEnvironment(ids[0]);
			}
			return NULL;
		}

		std::vector<int> getAlreadyExistingIds()
		{
			std::vector<int> result;
			for (auto& p : fs::directory_iterator("./assets/map/")) //parcours du dossier contenant les maps
			{
				try
				{
					int id = std::stoi(p.path().filename().stem().string());
					if (id > 0)
					{
						result.push_back(id);
					}
				}
				catch (std::exception &e)
				{

				}
			}
				
			return result;
		}

		int getAvailableId()
		{
			std::vector<int> usedIds = getAlreadyExistingIds();
			int max = 0;

			for (int i = 0; i < usedIds.size(); i++)
			{
				if (usedIds[i] > max)
				{
					max = usedIds[i];
				}
			}

			return max + 1;
		}
	};
}