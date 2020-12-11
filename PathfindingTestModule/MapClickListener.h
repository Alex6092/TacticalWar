#pragma once

#include "RendererEventListener.h"
#include "TestColorator.h"
#include <Environment.h>

class MapClickListener : public tw::RendererEventListener
{
	TestColorator * colorator;

	bool startPointSelected;
	tw::Point2D startCell;
	bool endPointSelected;
	tw::Point2D endCell;
	tw::Environment * environment;
	std::vector<tw::Point2D> processedPath;

public:
	MapClickListener(tw::Environment * e, TestColorator * colorator);
	~MapClickListener();

	virtual void onCellClicked(int x, int y);
	virtual void onCellHover(int x, int y);
	virtual void onCellMouseDown(int x, int y);
};

