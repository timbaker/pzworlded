#ifndef WATERFLOW_H
#define WATERFLOW_H

class WorldDocument;

class WaterFlow
{
public:
    WaterFlow();

    void readOldWaterDotLua(WorldDocument *worldDoc);
};

#endif // WATERFLOW_H
