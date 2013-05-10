#ifndef TILE_WRITE_H
#define TILE_WRITE_H

#include <string>
#include <stdio.h>
#include <vector>
#include <QObject>
#include <QString>
#include "types.h"

struct placename {
    double tilex, tiley; // fractional x/y tile coords.
    char type;
    std::string name;
};

class DrawingRules {
  public:
    struct DrawingRule {
        osm_type_t type_from; //List of types the rule
        osm_type_t type_to;   //applies to
        bool polygon;
        int zoom_from, zoom_to;
        int red[2], green[2], blue[2];
        double width[2];
    };
	 DrawingRules(const std::string &filename);
    bool get_rule(osm_type_t type, int zoom, int pass,
                  int *r, int *g, int *b, double *width, bool *polygon);
  private:
    static void tokenise(const std::string &input, std::vector<std::string> &output);
    bool load_rules();

	 std::string filename;
    std::vector<struct DrawingRule> drawing_rules;
};

class TileWriter : public QObject {
	Q_OBJECT

public slots:

	bool draw_image( QString filename, int x, int y, int zoom, int magnification);

signals:

	void image_finished( int x, int y, int zoom, int magnification, QByteArray data );

public:
	TileWriter( QString dir );
	virtual ~TileWriter() {}

	void get_placenames(int x, int y, int zoom, int drawzoom, std::vector<struct placename> &result) const;
private:
	bool query_index(int x, int y, int zoom, int cur_db, int *nways) const;
	static bool need_next_pass(int type1, int type2);
	class ImgWriter *img;
	std::string filename[3];
	class qindex *qidx[3];
	FILE *db[3];
	DrawingRules dr;
};

#endif
