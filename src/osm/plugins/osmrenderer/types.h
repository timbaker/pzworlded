#ifndef OSM_TYPES_H
#define OSM_TYPES_H

enum osm_type_t {
    AREA_PARK =             1,
    AREA_CAMPSITE =         2,
    AREA_NATURE =           3,
    AREA_CEMETERY =         4,
    AREA_RESIDENTIAL =      5,
    AREA_BARRACKS =         6,
    AREA_MILITARY =         7,
    AREA_FIELD =            8,
    AREA_DANGER_AREA =      9,
    AREA_MEADOW =          10,
    AREA_COMMON =          11,
    AREA_FOREST =          12,
    AREA_WATER =           13,
    AREA_WOOD =            14,
    AREA_RETAIL =          15,
    AREA_INDUSTRIAL =      16,
    AREA_PARKING =         16,
    AREA_BUILDING =        17,
   
    HW_PEDESTRIAN =       101,
    HW_PATH =             102,
    HW_FOOTWAY =          103,
    HW_STEPS =            104,
    HW_BRIDLEWAY =        105,
    HW_CYCLEWAY =         106,
    HW_PRIVATE =          107,
    HW_UNSURFACED =       108,
    HW_UNCLASSIFIED =     109,
    HW_RESIDENTIAL =      110,
    HW_LIVING_STREET =    111,
    HW_SERVICE =          112,
    HW_TERTIARY =         113,
    HW_SECONDARY =        114,
    HW_PRIMARY =          115,
    HW_TRUNK =            116,
    HW_MOTORWAY =         117,

    RW_RAIL =             132,
    WATERWAY =            164,

    PLACE_TOWN =          200,
    DONE =                255
};

#endif
