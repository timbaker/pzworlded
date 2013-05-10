/*
Copyright 2010  Christian Vetter veaac.fdirct@gmail.com

This file is part of MoNav.

MoNav is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

MoNav is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with MoNav.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "qtilerenderer.h"
#include "utils/qthelpers.h"
#include "xmlreader.h"
#ifndef NOPBF
#include "pbfreader.h"
#endif
#ifndef NOGUI
#include "qrsettingsdialog.h"
#endif

#include <QFile>
#include <QSettings>
#include <QDebug>
#include <stdio.h>
#include <string.h>
#include <map>
#include <vector>
#include <list>
#include <string>
#include <algorithm>
#include <time.h>

#define NEED_QTILE_WRITE //Need this before including quadtile.h
#include "quadtile.h"
#include "types.h"

//Index granularity. Index is terminated at max depth, or when a part of the
//index tree contains max_index_contents ways, whichever is the sooner.
//A significant part of the preprocessing time scales with 4^MAX_INDEX_DEPTH
//#define MAX_INDEX_DEPTH 16
#define MAX_INDEX_DEPTH 15
#define MAX_INDEX_CONTENTS 100

//Split ways that cross tiles this size at the tile boundaries.
int g_waysplit_tiledepth=13;

//Removes last 8 bits from a qtile to round it down for improved storage.
#define QTILE_LB_MASK (~(0xFFULL))
//Way segments longer than this can't be stored in the database format, and
//will need an interpolated node inserted to split them.
#define LONGEST_WAY_SEG (1<<19)

#define DB_VERSION "org.hollo.quadtile.pqdb.03"

#define qls(x) QLatin1String(x)

using std::vector;
using std::map;
using std::string;
using std::list;

struct stats {
	 std::map<std::string, long> bytes_used;
};
struct stats g_stats;

struct osm_subrule_t {
	 const char *val;
	 osm_type_t type;
};
struct osm_rules_t {
	 const char *key;
	 const osm_subrule_t *subrules;
};

struct osm_subrule_t waterway_rules[] = {
  {"stream",           WATERWAY},
  {"river",            WATERWAY},
  {"canal",            WATERWAY},
  {"ditch",            WATERWAY},
  {"drain",            WATERWAY},
  {"riverbank",        AREA_WATER},
  {0, DONE}
};
struct osm_subrule_t railway_rules[] = {
 {"rail",              RW_RAIL},
 {"light_rail",        RW_RAIL},
 {"narrow_gauge",      RW_RAIL},
 {"tram",              RW_RAIL},
 {"monorail",          RW_RAIL},
 {"subway",            RW_RAIL},
 {"preserved",         RW_RAIL},
 {0, DONE}
};
struct osm_subrule_t landuse_rules[] = {
 {"vineyard",          AREA_NATURE},
 {"cemetery",          AREA_CEMETERY},
 {"grave_yard",        AREA_CEMETERY},
 {"residential",       AREA_RESIDENTIAL},
 {"military",          AREA_MILITARY},
 {"field",             AREA_FIELD},
 {"meadow",            AREA_MEADOW},
 {"grass",             AREA_MEADOW},
 {"conservation",      AREA_COMMON},
 {"recreation_ground", AREA_COMMON},
 {"forest",            AREA_FOREST},
 {"wood",              AREA_WOOD},
 {"retail",            AREA_RETAIL},
 {"commercial",        AREA_RETAIL},
 {"industrial",        AREA_INDUSTRIAL},
 {"railway",           AREA_INDUSTRIAL},
 {0, DONE}
};
struct osm_subrule_t leisure_rules[] = {
 {"common",            AREA_COMMON},
 {"garden",            AREA_COMMON},
 {"pitch",             AREA_COMMON},
 {"park",              AREA_PARK},
 {"nature_reserve",    AREA_NATURE},
 {0, DONE}
};
struct osm_subrule_t amenity_rules[] = {
 {"parking",           AREA_PARKING},
 {0, DONE}
};
struct osm_subrule_t tourism_rules[] = {
 {"camp_site",         AREA_CAMPSITE},
 {"attraction",        AREA_CAMPSITE},
 {"picnic_site",       AREA_CAMPSITE},
 {0, DONE}
};
struct osm_subrule_t military_rules[] = {
 {"barracks",          AREA_BARRACKS},
 {"danger_area",       AREA_DANGER_AREA},
 {0, DONE}
};
struct osm_subrule_t natural_rules[] = {
 {"wood",              AREA_WOOD},
 {"water",             AREA_WATER},
 {"heath",             AREA_COMMON},
 {"field",             AREA_FIELD},
 {0, DONE}
};
struct osm_subrule_t building_rules[] = {
 {"yes",              AREA_BUILDING},
 {0, DONE}
};
struct osm_subrule_t highway_rules[] = {
 { "pedestrian",       HW_PEDESTRIAN },
 { "path",             HW_PATH },
 { "footway",          HW_FOOTWAY },
 { "steps",            HW_STEPS },
 { "bridleway",        HW_BRIDLEWAY },
 { "cycleway",         HW_CYCLEWAY },
 { "private",          HW_PRIVATE },
 { "unsurfaced",       HW_UNSURFACED },
 { "unclassified",     HW_UNCLASSIFIED },
 { "residential",      HW_RESIDENTIAL },
 { "living_street",    HW_LIVING_STREET },
 { "service",          HW_SERVICE },
 { "tertiary",         HW_TERTIARY },
 { "tertiary_link",    HW_TERTIARY },
 { "secondary",        HW_SECONDARY },
 { "secondary_link",   HW_SECONDARY },
 { "primary",          HW_PRIMARY },
 { "primary_link",     HW_PRIMARY },
 { "trunk",            HW_TRUNK },
 { "trunk_link",       HW_TRUNK },
 { "motorway",         HW_MOTORWAY },
 { "motorway_link",    HW_MOTORWAY },
 { 0, DONE }
};
struct osm_subrule_t tracktype_rules[] = {
 { "grade1",          HW_SERVICE },
 { "grade2",          HW_UNSURFACED },
 { "grade3",          HW_PATH },
 { 0, DONE }
};

struct osm_rules_t osm_rules[] = {
  {"waterway", waterway_rules},
  {"railway", railway_rules},
  {"landuse", landuse_rules},
  {"leisure", leisure_rules},
  {"amenity", amenity_rules},
  {"tourism", tourism_rules},
  {"military", military_rules},
  {"natural", natural_rules},
  {"building", building_rules},
  {"highway", highway_rules},
  {"tracktype", tracktype_rules},
  {0, 0}
};

struct projectedxy {
	 unsigned long x, y;
};

class osm_way {
  public:
	osm_way() { m_type=0; nnodes=0;};
	static bool sorter(const osm_way &w1, const osm_way &w2);

	unsigned long offset;
	unsigned char m_type;
	//std::string name;
	quadtile _q; //The point that will be used to insert this way in the index.

	int store(vector<class osm_way> &wlist, int splitlevel,
					std::string &stats_key);
	void interpolate_long_ways();
	bool is_worth_saving(bool motorway=false);
	int buf_len();
	quadtile q() const {return _q;};
	bool is_oneway();
	void get_buf(unsigned char *buf);//Call buf_len first. buf must be this big.
	unsigned char type();
//All nodes are stored in a global vector to avoid memory fragmentation.
	static vector<struct projectedxy> all_nodes;
	int inodes, nnodes;
};
vector<struct projectedxy> osm_way::all_nodes;

class qindexTree {
	 public:
		  qindexTree();
		  void addIndex(quadtile _q, long long _offset);
		  void addIndex_r(quadtile _q, long long _offset, int entries=1);
		  void print(FILE *fp=stdout);
		  void increase_offsets(long off) {
				offset += off;
				for(int i=0;i<4;i++) if(child[i]) child[i]->increase_offsets(off);
		  };
		  void deleteRecursive();

	 private:
		  bool contains(quadtile testq) {
				return((testq & (~qmask))==q);
		  };
		  qindexTree(quadtile _q, quadtile _qmask, int _level, qindexTree *_parent = NULL);
		  void recursive_fill_nulls();
		  void subprint(FILE *fp, int *index);

		  qindexTree *child[4];
		  qindexTree *parent;
		  long long q, qmask, lastq;
		  int level;
		  long offset;
		  int my_index;
		  int contents;
		  int deferred_contents;
		  static int index_entry_size;
};
int qindexTree::index_entry_size=28; //No. of bytes each index entry takes up.

/* Default constructor - used for the top node of the tree only. */
qindexTree::qindexTree()
{
	 for(int i=0; i<4; i++) child[i]=0;
	 q = 0; lastq=0; deferred_contents = 0;
	 qmask = 0x3FFFFFFFFFFFFFFFLL;
	 //binary_printf(q); printf("\n");
	 level = 0;
	 offset = -1; contents=0; my_index = -1;
	 parent = NULL;
}

/* Private constructor for adding a child node */
qindexTree::qindexTree(quadtile _q, quadtile _qmask, int _level, qindexTree *_parent)
	 : parent(_parent), q(_q), qmask(_qmask), level(_level)
{
	 for(int i=0; i<4; i++) child[i]=0;
	 offset = -1; contents = 0; my_index = -1; lastq=0; deferred_contents=0;

	 //printf("New quadtile: %llx %llx %d\n", q, qmask, level);
	 //binary_printf(q); printf("\n");
	 //binary_printf(qmask); printf("\n");
}

/* Fill any NULL children with a qindexTree structure with no ways. */
void qindexTree::recursive_fill_nulls()
{
	 if(level >= MAX_INDEX_DEPTH) return; //Maximum depth of tree.
	 for(int i=0;i<4;i++) {
		  if(!child[i]) {
				quadtile newq = q | (((long long)i) << (60-(level*2)));
				child[i] = new qindexTree(newq, qmask >> 2, level+1, this);
		  }
	 }
}

/* deletes the whole tree */
void qindexTree::deleteRecursive()
{
	if ( level >= MAX_INDEX_DEPTH )
		return;

	for ( int i = 0; i < 4; i++ ) {
		if ( child[i] != NULL )
			child[i]->deleteRecursive();
		delete child[i];
	}
}

/* Calls the recursive addIndex function if it will make a difference. */
void qindexTree::addIndex(quadtile _q, long long _offset)
{
	 quadtile _qmask = ~(0x3FFFFFFFFFFFFFFFLL >> (2*MAX_INDEX_DEPTH));
	 if((lastq & _qmask)==(_q & _qmask)) {
		  deferred_contents++;
	 } else {
		  if(deferred_contents) addIndex_r(lastq, -1, deferred_contents);
		  addIndex_r(_q, _offset);
		  deferred_contents = 0;
	 }
	 lastq = _q;
}

void qindexTree::addIndex_r(quadtile _q, long long _offset, int entries)
{
	 //if(level==0) {
		  //printf("\nNew addIndex for %llx ", _q);
		  //binary_printf(_q); printf("\n");
	 //}
	 if(!contains(_q)) {
		  printf("ERROR - asked to add an index for a qtile we don't contain\n");
		  exit(-1);
	 }

	 contents+=entries;
	 if(offset==-1) {
		  if(_offset==-1) {printf("ERROR - addIndex optimisation broken down\n"); exit(-1);}
		  offset = _offset;
	 }
	 else if(_offset < offset && _offset!=-1) {
		  printf("ERROR - addIndex called out of order\n");
	 }

	 if(level >= MAX_INDEX_DEPTH) return; //Maximum depth of tree.

	 for(int i=0; i<4; i++) {
		  if(!child[i]) {
				quadtile newq = q | (((long long)i) << (60-(level*2)));
				child[i] = new qindexTree(newq, qmask >> 2, level+1, this);
		  }

		  if(child[i]->contains(_q)) return child[i]->addIndex_r(_q, _offset, entries);
	 }
	 printf("Error - we (%llx, %d) contain %llx, but none of our children claim it\n", q, level, _q);
	 //binary_printf(q); printf("\n"); binary_printf(_q); printf("\n");
	 exit(-1);
}

void qindexTree::print(FILE *fp)
{
	 int index=0;

	 recursive_fill_nulls();
	 subprint(NULL, &index);

	 //Would need to go to 4 bytes in storage format.
	 if(index > 255*255*255 - 3) throw("Too many index entries\n");

	 //Change offsets to be from beginning of the file (taking into account the
	 //index size).
	 long index_bytes = index_entry_size*index + 3;
	 increase_offsets(index_bytes);

	 unsigned char *s = ll2buf(index);
	 if(fwrite(s+5, 3, 1, fp)!=1) throw("Failed write\n");
	 subprint(fp, NULL);
}

/*Called in two passes. Pass 1 fp==NULL, pass 2 index==NULL.
  On pass 1 we do everything except write the file, and index the
  qindexTree structures with the order in which they will be written.
  On pass 2 we write the file, using the index of the child as the
  pointer when serialising child[4].
  index 0xFFFFFF means we have too few ways ourself, and haven't bothered
  with children. 0xFFFFFE means the child has no ways and doesn't exist.*/
void qindexTree::subprint(FILE *fp, int *index)
{
	 //if(contents==0) return;
	 if(index) {
		  my_index = *index;
		  (*index)++;
	 }
	 if(fp) {
		  unsigned char *s;
		  s = ll2buf(q);
		  if(fwrite(s, 8, 1, fp)!=1) throw("Failed write\n");
		  s = ll2buf(offset);
		  if(fwrite(s+4, 4, 1, fp)!=1) throw("Failed write\n");
		  s = ll2buf(contents);
		  if(fwrite(s+4, 4, 1, fp)!=1) throw("Failed write\n");
		  //unsigned char c = (contents > 255) ? 0 : ((unsigned char) contents);
		  //if(fwrite(&c, 1, 1, fp)!=1) throw("Failed write\n");
	 }
	 if(parent && parent->contents < MAX_INDEX_CONTENTS) {
	 //Need to terminate the tree on parents contents not ours. We could
	 //contain no ways while our parent contains many thousands, so we
	 //need substantiating as an entry with 0 contents.
		  if(fp) {
				unsigned char idx_buf[3*4];
				memset(idx_buf, 255, 3*4);
				if(fwrite(idx_buf, 3, 4, fp)!=4) throw("Failed write\n");
		  }
		  return;
	 }
	 if(fp) {
		  for(int i=0;i<4; i++) {
				if(child[i]) {
					 unsigned char *s = ll2buf(child[i]->my_index);
					 if(fwrite(s+5, 3, 1, fp)!=1) throw("Failed write\n");
				} else { //Child doesn't exist
					 unsigned char idx_buf[] = {0xFF, 0xFF, 0xFE};
					 if(fwrite(idx_buf, 3, 1, fp)!=1) throw("Failed write\n");
				}
		  }
	 }
	 for(int i=0;i<4; i++) if(child[i]) child[i]->subprint(fp, index);
}

bool osm_way::is_worth_saving(bool motorway)
{
	 if(m_type==0) return false;
	 if(motorway && m_type<=HW_SECONDARY && m_type>=HW_PEDESTRIAN) return false;
	 if(motorway && m_type <= HW_PEDESTRIAN) return false; //Trial of this
	 if(motorway && m_type == WATERWAY) return false; //Trial of this
	 return true;
}

int osm_way::buf_len()
{
	if(!is_worth_saving()) {
		printf("Trying to write an inappropriate way\n");
		exit(-1);
	}
	//buf is made up from:
		//Type         - 1 byte
		//No of coords - 2  bytes
		//First coordinate - 8bytes
		//Offsets to subsequent nodes - 4 bytes each.
	return 3 + 8 + 4 * (nnodes-1);
}

unsigned char osm_way::type()
{
	 return m_type;
}

void osm_way::get_buf(unsigned char *buf)
{
	 int i = buf_len();
	 if(i==0) return;
	 memset(buf, 0, i);

	 //Serialise type and no. of nodes
	 buf[0] = type();
	 memcpy(buf+1, l2buf(nnodes)+2, 2);

	 //x/y coords of the first node.
	 quadtile x, y, lastx, lasty;
	 lastx = all_nodes[inodes].x; lasty = all_nodes[inodes].y;
	 memcpy(buf+3, ll2buf((lastx<<32LL) | lasty), 8);

	 //Now offsets to the subsequent nodes.
	 i = 0;
	 for(int j = inodes+1; j<inodes+nnodes; j++) {
		  x = all_nodes[j].x; y = all_nodes[j].y;

		  long xdiff = x-lastx, ydiff = y-lasty;
		  if(xdiff<-LONGEST_WAY_SEG || xdiff>LONGEST_WAY_SEG ||
			  ydiff<-LONGEST_WAY_SEG || ydiff>LONGEST_WAY_SEG) {
				printf("Too long a segment %ld %ld\n", xdiff, ydiff);
				exit(-1);
		  }
		  xdiff += LONGEST_WAY_SEG;
		  ydiff += LONGEST_WAY_SEG;
		  xdiff >>= 4; ydiff >>= 4;
		  memcpy(buf + 11 + i*4, l2buf(xdiff)+2, 2);
		  memcpy(buf + 11 + i*4 + 2, l2buf(ydiff)+2, 2);
		  lastx = x; lasty = y;
		  i++;
	 }
}

/* Insert interpolated nodes wherever the jump between two nodes is greater
	than LONGEST_WAY_SEG. We insert into vector all_nodes - this would
   invalidate all pointers into all_nodes from subsequent ways, so only ever
   call on the most recently added way. This restriction means performance
   isn't an issue.*/
void osm_way::interpolate_long_ways()
{
	quadtile x, y, oldx, oldy, newx, newy;
	for(int i=inodes; i<inodes+nnodes; i++) {
		x = all_nodes[i].x; y = all_nodes[i].y;
		if(i!=inodes) {
				long xdiff = x-oldx, ydiff = y-oldy;
				if(labs(xdiff)>=LONGEST_WAY_SEG || labs(ydiff)>=LONGEST_WAY_SEG) {
					long xstep = (x<oldx ? 1-LONGEST_WAY_SEG : LONGEST_WAY_SEG-1);
					long ystep = (y<oldy ? 1-LONGEST_WAY_SEG : LONGEST_WAY_SEG-1);

					if(labs(xdiff) > labs(ydiff)) {
							newx = oldx + xstep;
							newy= oldy + (newx-(double)oldx) * (y-(double)oldy)
												  / (x-(double)oldx);
					} else {
							newy = oldy + ystep;
							newx=oldx + (newy-(double)oldy) * (x-(double)oldx)
												/ (y-(double)oldy);
					}
					struct projectedxy xy;
					//Round x and y individually instead of useing QTILE_LB_MASK
					xy.x = newx & (~0xFULL); xy.y = newy & (~0xFULL);
					all_nodes.insert(all_nodes.begin()+i, xy);
									nnodes++;
					x=newx; y=newy; //So oldx/oldy point to the new node.
				}
		}
		oldx = x; oldy = y;
	}
}

/* Store the way into wlist. Split level is the level of the smallest
	supported quadtiles. We guarantee that no way crosses from one of
	these quadtiles to another. Return the total number of qt ways stored */
int osm_way::store(vector<class osm_way> &wlist, int splitlevel,
						 std::string &stats_key)
{
	int result = 1;
	quadtile qmask = 0x3FFFFFFFFFFFFFFFLL >> (splitlevel*2);
	quadtile qm = q() & (~qmask);
	interpolate_long_ways();
	if(m_type<100) { //Areas need splitting differently.
		  //We don't do this very cleverly - area may end up being stored in
		  //places it isn't needed.
		  quadtile minx=0, miny=0, maxx=0, maxy=0, x, y;
		  for(int i = inodes; i<inodes+nnodes; i++) {
				//demux(*i, &x, &y);
				x = all_nodes[i].x; y = all_nodes[i].y;
				if(minx==0 || x<minx) minx = x;
				if(miny==0 || y<miny) miny = y;
				if(maxx==0 || x>maxx) maxx = x;
				if(maxy==0 || y>maxy) maxy = y;
		  }
		  //Really inefficient. FIXME if it matters. Also the >>2 of qmask and
		  //splitlevel-1 shouldn't be needed but are - must have got the maths
		  //wrong somewhere. FIXME
		  quadtile top_left     = mux(minx, miny) & (~(qmask>>2));
		  quadtile bottom_right = mux(maxx, maxy) & (~(qmask>>2));
		  demux(top_left, &minx, &miny);
		  demux(bottom_right, &maxx, &maxy);

		  quadtile splittilesize = 1<<(31-splitlevel-1);
		  //printf("  %lld %lld %lld\n", maxx-minx, maxy-miny, splittilesize);
		  for(x=minx; x<=maxx; x+=splittilesize) {
				for(y=miny; y<=maxy; y+=splittilesize) {
					 quadtile newq = mux(x, y);
					  osm_way newarea(*this);
					  newarea._q = newq;
					  result++;
					  g_stats.bytes_used[stats_key] += newarea.buf_len();
					  wlist.push_back(newarea);
				}
		  }
		  return result;
	}
	for(int i=inodes; i<inodes+nnodes; i++) {
		quadtile iq = mux(all_nodes[i].x, all_nodes[i].y);
		if(i!=inodes && ((iq & (~qmask)) != qm)) {
			qm = iq & (~qmask);
			class osm_way w2;
			w2._q = qm;
			w2.m_type = m_type;
			w2.inodes = i-1;
			w2.nnodes = nnodes - (i-inodes-1);
			nnodes = i-inodes+1;
			wlist.push_back(w2);
			w2.store(wlist, splitlevel, stats_key);
		}
	}
	g_stats.bytes_used[stats_key] += buf_len();
	wlist.push_back(*this);
	return result;
}

bool osm_way::sorter(const osm_way &w1, const osm_way &w2)
{
	return(w1.q() < w2.q());
}

/* *****************************************
 *             XML Parsing
 * *****************************************/
struct node {
	 int id;
	 unsigned long x, y;
};

class placename {
  public:
	 enum {CITY=0, TOWN=1, VILLAGE=2, STATION=3, SUBURB=4, HAMLET=5}  type;
	 quadtile position;
	 QString name;
	 static bool sorter(const placename &p1, const placename &p2)
		{return (p1.position<p2.position);};
};

class OSMReader {
  public:
	 OSMReader() {};
	 bool load(const QString &filename);
	 vector<class osm_way> &get_ways() {return ways;};
	 vector<placename> &get_places() {return placenames;};
	 void delete_ways();

  private:
	 bool load_xml(const QString &filename);
	 void add_node(IEntityReader::Node &node);
	 void add_way(IEntityReader::Way &w);
	 bool get_node(int id, unsigned long *x, unsigned long *y);

	 static bool nodesorter(const struct node &n1, const struct node &n2);

	 vector<struct node> nodes;
	 vector<class osm_way> ways;
     vector<placename> placenames;
};

bool OSMReader::load(const QString &filename)
{
	Timer timer;
     qDebug() << "Qtile: Loading osm file:";
	 if(!load_xml(filename)) return false;
     qDebug() << "Qtile: Loaded osm file: " << timer.restart() << "ms";

	 //Ways are all loaded - no longer need the nodes so clear them for memory.
     qDebug() << "Qtile: Clearing nodes";
	 std::vector< struct node >().swap( nodes );
     qDebug() << "Qtile: Cleared nodes: " << timer.restart() << "ms";

	 qDebug() << "Qtile: Sorting ways";
	 std::sort(ways.begin(), ways.end(), osm_way::sorter);
     qDebug() << "Qtile: Sorted ways: " << timer.restart() << "ms";

	 qDebug() << "Qtile: Sorting places";
	 std::sort(placenames.begin(), placenames.end(), placename::sorter);
     qDebug() << "Qtile: Sorted places: " << timer.restart() << "ms";

	 return true;
}

bool OSMReader::load_xml(const QString &filename)
{
	IEntityReader* reader = NULL;
    if ( filename.endsWith( qls("osm.bz2") ) || filename.endsWith( qls(".osm") ) )
		reader = new XMLReader();
#ifndef NOPBF
	else if ( filename.endsWith( ".pbf" ) )
		reader = new PBFReader();
#endif

	if ( reader == NULL ) {
		qCritical() << "File format not supported";
		return false;
	}

	 if(!reader->open(filename)) {
		  fprintf(stderr, "Failed to open file\n");
		  delete reader;
		  return false;
	 }

	 QStringList list;
     list.push_back(qls("name"));
	 for(osm_rules_t *rule=osm_rules; rule->subrules;rule++)
          list.push_back(qls(rule->key));
	 reader->setWayTags(list);
	 list.clear();
     list.push_back(qls("place"));
     list.push_back(qls("name"));
     list.push_back(qls("railway"));
	 reader->setNodeTags(list);

	 IEntityReader::EntityType etype;
	 IEntityReader::Node n; IEntityReader::Way w; IEntityReader::Relation r;
	bool firstWay = true;
	 do {
		  etype = reader->getEntitiy(&n, &w, &r);
		  switch(etype) {
		  case IEntityReader::EntityNode:
				add_node(n);
				break;
		  case IEntityReader::EntityWay:
				if(firstWay) qDebug() << "Qtile: Loading osm ways:";
				firstWay = false;
				add_way(w);
				break;
		  case IEntityReader::EntityRelation:
				//Not interested in relations.
				break;
		  case IEntityReader::EntityNone:
				break;
		  }
	 } while(etype!=IEntityReader::EntityNone);
	 delete reader;

	 return true;
}

bool OSMReader::nodesorter(const struct node &n1, const struct node &n2)
{
	 return n1.id < n2.id;
}

void OSMReader::add_way(IEntityReader::Way &way)
{
	 if(way.nodes.size()<2) {
		  //printf("Excluding zero length way\n");
		  return;
	 }
	 class osm_way w;
	 w.m_type = 0;
	 std::string stats_key="";
	 // FIXME earlier code had tags in a map. MoNav's parsing delivers them
	 // in vector<Tag>. Temporary fix is to recreate the tagMap...
	 std::map<std::string, std::string> tagMap;
	 for(std::vector<IEntityReader::Tag>::iterator i = way.tags.begin();
				i!=way.tags.end(); i++) {
		  std::string s(i->value.toAscii());
		  if(!s.size()) continue;
		  if(i->key) tagMap[osm_rules[i->key - 1].key] = s;
		  else tagMap["name"] = s;
	 }
	 for(osm_rules_t *rules=osm_rules; rules->subrules && !w.m_type;rules++) {
		  const char *key = rules->key;
		  std::string val = tagMap[key];
		  for(const osm_subrule_t *srules = rules->subrules;
				srules->val && !w.m_type; srules++) {
				if(val==srules->val) {
					 w.m_type = srules->type;
					 stats_key = std::string(rules->key) + ":" + tagMap[key];
				}
		  }
	 }

	 if(!w.is_worth_saving()) return;

	 unsigned long x, y;
	 if(!get_node(*way.nodes.begin(), &x, &y))  return;
	 w._q = mux(x, y);
         w.inodes = osm_way::all_nodes.size();
	for(std::vector<unsigned int>::iterator i = way.nodes.begin();
			i != way.nodes.end(); i++) {
		//quadtile nq = get_node(*i);
		projectedxy xy;
		if(!get_node(*i, &xy.x, &xy.y)) return;
		osm_way::all_nodes.push_back(xy);
		w.nnodes++;
	}
	if((ways.size()%100000)==0)
		qDebug() << ways.size() << " ways with " << osm_way::all_nodes.size() << "nodes";
	w.store(ways, g_waysplit_tiledepth, stats_key);
}

void OSMReader::add_node(IEntityReader::Node &node)
{
	 struct node n;
	 n.id=node.id;
	 ll2pxy(node.coordinate.latitude, node.coordinate.longitude, &n.x, &n.y);
	 n.x = n.x & (~0xFULL);
	 n.y = n.y & (~0xFULL);
	 nodes.push_back(n);

         if((nodes.size()%1000000)==0)
                 qDebug() << nodes.size() << " nodes loaded";
std::map<QString, QString> tagMap;
	 for(std::vector<IEntityReader::Tag>::iterator i = node.tags.begin();
				i!=node.tags.end(); i++) {
		  if(!i->value.size()) continue;
		  switch(i->key) {
		  case 0:
                tagMap[qls("place")] = i->value;
				break;
		  case 1:
                tagMap[qls("name")] = i->value;
				break;
		  case 2:
                tagMap[qls("railway")] = i->value;
				break;
		  }
	 }
     if((tagMap[qls("place")].size() > 0 || tagMap[qls("railway")]==qls("station"))
          && tagMap[qls("name")].size() > 0) {
		  placename place;
          if(tagMap[qls("place")]==qls("city")) place.type = placename::CITY;
          else if(tagMap[qls("place")]==qls("town")) place.type = placename::TOWN;
          else if(tagMap[qls("place")]==qls("village")) place.type = placename::VILLAGE;
          else if(tagMap[qls("place")]==qls("suburb")) place.type = placename::SUBURB;
          else if(tagMap[qls("place")]==qls("hamlet")) place.type = placename::HAMLET;
          else if(tagMap[qls("railway")]==qls("station")) place.type = placename::STATION;
		  else return;
          place.name = tagMap[qls("name")];
		  place.position = mux(n.x, n.y);
		  placenames.push_back(place);
	 }
}

/*This gets a node by id from the nodes list. Assume no nodes are added
  after first call to this function. On first call we sort nodes and then
  do a binary search. This uses less memory and works out faster than using
  a std::map for nodes*/
bool OSMReader::get_node(int id, unsigned long *x, unsigned long *y)
{
	static bool sorted_nodes=false;
	if(!sorted_nodes) std::sort(nodes.begin(), nodes.end(), nodesorter);
	sorted_nodes=true;

	int i, bottom, top;
	bottom = 0;
	top = nodes.size();
	do {
		i = (top+bottom)/2;
		if(top-bottom==1) i=bottom;
		if(nodes[i].id==id) {
				*x = nodes[i].x; *y = nodes[i].y;
				return true;
		}
		if(top-bottom==1) {
			i=top;
			if(nodes[i].id!=id) {
				qDebug() << "Can't find node " << id;
				return false;
			}
			*x = nodes[i].x; *y = nodes[i].y;
			return true;
		}

		if(nodes[i].id<id) bottom = i;
		else top=i;

	} while(top!=bottom);
	qDebug() << "Can't find node " << id;
	return(false);
}

void OSMReader::delete_ways()
{
	ways.clear();
	osm_way::all_nodes.clear();
	std::vector<class osm_way>().swap( ways );
	std::vector<struct projectedxy>().swap( osm_way::all_nodes);
}

QtileRenderer::QtileRenderer()
{
        Q_INIT_RESOURCE(rendering_rules);
}

QtileRenderer::~QtileRenderer()
{
}

QString QtileRenderer::GetName()
{
    return qls("Qtile Renderer");
}

int QtileRenderer::GetFileFormatVersion()
{
	return 1;
}

QtileRenderer::Type QtileRenderer::GetType()
{
	return Renderer;
}

bool QtileRenderer::LoadSettings( QSettings* settings )
{
        m_settings.rulesFile = qls(":/rendering_rules/default.qrr");
	if ( settings == NULL )
		return false;
    settings->beginGroup( qls("Qtile Renderer") );
    m_settings.inputFile = settings->value( qls("input") ).toString();
    m_settings.rulesFile = settings->value( qls("rulesFile") ).toString();
    m_settings.unused = settings->value( qls("unused") ).toBool();
	settings->endGroup();
        if(m_settings.rulesFile.size()==0)
            m_settings.rulesFile = qls(":/rendering_rules/default.qrr");

	return true;
}

bool QtileRenderer::SaveSettings( QSettings* settings )
{
	if ( settings == NULL )
		return false;
    settings->beginGroup( qls("Qtile Renderer") );
    settings->setValue( qls("input"), m_settings.inputFile );
    settings->setValue( qls("rulesFile"), m_settings.rulesFile );
    settings->setValue( qls("unused"), m_settings.unused );
	settings->endGroup();

	return true;
}

bool QtileRenderer::Preprocess( IImporter*, QString dir )
{
	m_osr = new OSMReader;

		  qDebug() << "Qtile renderer preprocessing";
                  QString ofile_name = dir + qls("/rendering.qrr");
		  QFile infile(m_settings.rulesFile), outfile(ofile_name);
                  if(!infile.open(QIODevice::ReadOnly)) {
                          qCritical() << "Failed to open rendering rules: " << m_settings.rulesFile;
                          return false;
		  }
                  if(!outfile.open(QIODevice::WriteOnly)) {
                          qCritical() << "Failed to open rendering rules output file: " << ofile_name;
                          return false;
		  }
                  while(!infile.atEnd()) outfile.write(infile.readLine());
                  infile.close(); outfile.close();
		  m_osr->load(m_settings.inputFile);
		  write_ways(dir, false);
		write_ways(dir, true);
		write_placenames(dir);
		m_osr->delete_ways();
		qDebug() << "Qtile: preprocessing finished";

	delete m_osr;

	return true;
}

void QtileRenderer::write_ways(QString &dir, bool motorway)
{
	 //Calculate Offsets and index
	 //fprintf(stderr, "Calculating way file offsets\n");
	Timer timer;
	qDebug() << "Qtile: Calculating index";
	 long file_offset = 0;
	 qindexTree qidx;
	 for(vector<class osm_way>::iterator w = m_osr->get_ways().begin();
									w!=m_osr->get_ways().end(); w++) {
		  if(!w->is_worth_saving(motorway)) continue;
		  w->offset = file_offset;
		  file_offset += w->buf_len();
		  qidx.addIndex(w->q(), file_offset);
	 }

	 QString outfile = dir;
     outfile += qls(motorway ? "/ways.motorway.pqdb": "/ways.all.pqdb");
	 FILE *way_fp = fopen(outfile.toAscii(), "wb");

	qDebug() << "Qtile: index calculated: " << timer.restart() << "ms";
	qDebug() << (motorway ? "Qtile: Writing motorways":"Qtile: Writing ways:");
	 char tmp[100];
	 time_t t;
	 time(&t);
	 struct tm *_tm = gmtime(&t);
	 sprintf(tmp, "%s depth=%d %04d-%02d-%02d\n", DB_VERSION, g_waysplit_tiledepth,
						_tm->tm_year+1900, _tm->tm_mon+1, _tm->tm_mday);
	 fprintf(way_fp, "%s", tmp);

	 qidx.increase_offsets(strlen(tmp));
	 qidx.print(way_fp);
	 qidx.deleteRecursive();

	unsigned char *buf=(unsigned char *) malloc( 1024 );
	unsigned bufferLength = 1024;
	for(vector<class osm_way>::iterator w = m_osr->get_ways().begin();
									w!=m_osr->get_ways().end(); w++) {
		  if(!w->is_worth_saving(motorway)) continue;
		  unsigned length = w->buf_len();
		  if ( length > bufferLength ) {
			  bufferLength = length;
			  buf = (unsigned char *) realloc(buf, length);
		  }
		  w->get_buf(buf);
		  if(fwrite(buf, length, 1, way_fp)!=1)
				throw("Failed write\n");
	 }
	free(buf);
	fclose(way_fp);
	qDebug() << "Qtile: written: " << timer.restart() << "ms";
}

void QtileRenderer::write_placenames(QString &dir)
{
	Timer timer;
	qDebug() << "Qtile: Writing place names:";
	long file_offset = 0;
	qindexTree qidx;
	for(vector<placename>::iterator i = m_osr->get_places().begin();
		i!=m_osr->get_places().end(); i++) {
		int len = i->name.size();
		if(len>100) len=100;
		qidx.addIndex(i->position, file_offset);
		file_offset += len+10;
	}
	QString outfile = dir;
    outfile += qls("/places.pqdb");
	FILE *place_fp = fopen(outfile.toAscii(), "wb");
	char buf[115];
	time_t t;
	time(&t);
	struct tm *_tm = gmtime(&t);
	sprintf(buf, "%s depth=%d places %04d-%02d-%02d\n", DB_VERSION, 
		g_waysplit_tiledepth, _tm->tm_year+1900, _tm->tm_mon+1,
		_tm->tm_mday);
	fprintf(place_fp, "%s", buf);

	qidx.increase_offsets(strlen(buf));
	qidx.print(place_fp);
	qidx.deleteRecursive();

	for(vector<placename>::iterator i = m_osr->get_places().begin();
		i!=m_osr->get_places().end(); i++) {
		memcpy(buf, ll2buf(i->position), 8);
		int len = i->name.size();
		if(len>100) len=100;
		buf[8] = len>100 ? 100 : len;
		buf[9] = (unsigned char) i->type;
		strncpy(buf+10, i->name.toAscii(), 100);
		fwrite(buf, len+10, 1, place_fp);
	}
	fclose(place_fp);
	qDebug() << "Qtile: Written place names: " << timer.restart() << "ms";
}


#ifndef NOGUI
bool QtileRenderer::GetSettingsWindow( QWidget** window )
{
	*window = new QRSettingsDialog();
	return true;
}

bool QtileRenderer::FillSettingsWindow( QWidget* window )
{
	QRSettingsDialog* settings = qobject_cast< QRSettingsDialog* >( window );
	if ( settings == NULL )
		return false;

	return settings->readSettings( m_settings );
}

bool QtileRenderer::ReadSettingsWindow( QWidget* window )
{
	QRSettingsDialog* settings = qobject_cast< QRSettingsDialog* >( window );
	if ( settings == NULL )
		return false;

	return settings->fillSettings( &m_settings );
}
#endif

QString QtileRenderer::GetModuleName()
{
	return GetName();
}

bool QtileRenderer::GetSettingsList( QVector< Setting >* settings )
{
    settings->push_back( Setting( qls(""), qls("qtile-input"), qls("osm/osm.pbf input file"), qls("filename") ) );
    settings->push_back( Setting( qls(""), qls("qtile-rendering-rules"), qls("rendering rules file"), qls("filename") ) );
	return true;
}

bool QtileRenderer::SetSetting( int id, QVariant data )
{
	switch( id ) {
	case 0 :
		m_settings.inputFile = data.toString();
		break;
	case 1 :
		m_settings.rulesFile = data.toString();
		break;
	default:
		return false;
	}

	return true;
}

#ifndef ZOMBOID
Q_EXPORT_PLUGIN2( qtilerenderer, QtileRenderer )
#endif
