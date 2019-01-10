/*
 * Copyright 2018, Tim Baker <treectrl@users.sf.net>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "mapboxglwidget.h"

#include <QDebug>

MapboxGLWidget::MapboxGLWidget(QWidget *parent)
    : QOpenGLWidget (parent)
{

}

MapboxGLWidget::~MapboxGLWidget()
{
    // Make sure we have a valid context so we
    // can delete the QMapboxGL.
    makeCurrent();
}

void MapboxGLWidget::keyPressEvent(QKeyEvent *ev)
{
    switch (ev->key()) {
    case Qt::Key_Tab:
        m_map->cycleDebugOptions();
        break;
    default:
        break;
    }

    ev->accept();
}

void MapboxGLWidget::mousePressEvent(QMouseEvent *ev)
{
    m_lastPos = ev->localPos();

    if (ev->type() == QEvent::MouseButtonPress) {
        if (ev->buttons() == (Qt::LeftButton | Qt::RightButton)) {
        }
    }

    if (ev->type() == QEvent::MouseButtonDblClick) {
        if (ev->buttons() == Qt::LeftButton) {
            m_map->scaleBy(2.0, m_lastPos);
        } else if (ev->buttons() == Qt::RightButton) {
            m_map->scaleBy(0.5, m_lastPos);
        }
    }

    ev->accept();
}

void MapboxGLWidget::mouseMoveEvent(QMouseEvent *ev)
{
    QPointF delta = ev->localPos() - m_lastPos;

    if (!delta.isNull()) {
        if (ev->buttons() == Qt::LeftButton && ev->modifiers() & Qt::ShiftModifier) {
            m_map->setPitch(m_map->pitch() - delta.y());
        } else if (ev->buttons() == Qt::LeftButton) {
            m_map->moveBy(delta);
        } else if (ev->buttons() == Qt::RightButton) {
            m_map->rotateBy(m_lastPos, ev->localPos());
        }
    }

    m_lastPos = ev->localPos();
    ev->accept();
}

void MapboxGLWidget::wheelEvent(QWheelEvent *ev)
{
    if (ev->orientation() == Qt::Horizontal) {
        return;
    }

    float factor = ev->delta() / 1200.;
    if (ev->delta() < 0) {
        factor = factor > -1 ? factor : 1 / factor;
    }

    m_map->scaleBy(1 + factor, ev->pos());
    ev->accept();
}

static const QString styleJson = QStringLiteral(
R"JSON(
{
    "version": 8,
    "name": "pz",
    "sources": {
        "world1-buildings": {
            "type":"geojson",
            "data": {
                "type": "Feature",
                "geometry": {
                    "type": "Point",
                    "coordinates": [-77.0323, 38.9131]
                }
            }
        },
"world1-roads": {
    "type":"geojson",
    "data": {
        "type": "Feature",
        "geometry": {
            "type": "Point",
            "coordinates": [-77.0323, 38.9131]
        }
    }
},
"world1-landuse": {
    "type":"geojson",
    "data": {
        "type": "Feature",
        "geometry": {
            "type": "Point",
            "coordinates": [-77.0323, 38.9131]
        }
    }
},
"world1-poi_label": {
    "type":"geojson",
    "data": {
        "type": "Feature",
        "geometry": {
            "type": "Point",
            "coordinates": [-77.0323, 38.9131]
        }
    }
},
"world1-place_label": {
    "type":"geojson",
    "data": {
        "type": "Feature",
        "geometry": {
            "type": "Point",
            "coordinates": [-77.0323, 38.9131]
        }
    }
},
"world1-water": {
    "type":"geojson",
    "data": {
        "type": "Feature",
        "geometry": {
            "type": "Point",
            "coordinates": [-77.0323, 38.9131]
        }
    }
}

    },
    "sprite": "file://D:/pz/worktree/build40-weather/workdir/media/mapbox/sprites/bright-v8",
    "glyphs": "file://D:/pz/worktree/build40-weather/workdir/media/mapbox/glyphs/{fontstack}/{range}.pbf",
    "layers": [
        {
            "id": "background",
            "type": "background",
            "paint": {
                "background-pattern": "natural_paper"
            },
            "interactive": true
        },
        {
            "id": "world1-landuse_wood",
            "type": "fill",
            "source": "world1-landuse",
            "source-layer": "landuse",
            "filter": [
                "==",
                "natural",
                "wood"
            ],
            "paint": {
                "fill-color": "rgb(193,236,176)",
                "fill-opacity": 1.0
            },
            "interactive": true
        },
        {
            "interactive": true,
            "layout": {
                "line-cap": "round"
            },
            "filter": [
                "all",
                [
                    "!=",
                    "class",
                    "river"
                ],
                [
                    "!=",
                    "class",
                    "stream"
                ],
                [
                    "!=",
                    "class",
                    "canal"
                ]
            ],
            "type": "line",
            "source": "world1-water",
            "id": "world1-waterway",
            "paint": {
                "line-color": "#a0c8f0",
                "line-width": {
                    "base": 1.3,
                    "stops": [
                        [
                            13,
                            0.5
                        ],
                        [
                            20,
                            2
                        ]
                    ]
                }
            },
            "source-layer": "water"
        },
        {
            "id": "world1-water",
            "type": "fill",
            "filter": [
                "==",
                "$type",
                "Polygon"
            ],
            "source": "world1-water",
            "source-layer": "water",
            "paint": {
                "fill-pattern": "water"
            },
            "interactive": true
        },
        {
            "id": "world1-building_shadow",
            "type": "line",
            "source": "world1-buildings",
            "source-layer": "buildings",
            "minzoom": 15,
            "paint": {
                "line-pattern": "line_shade_22",
                "line-width": [
                    "interpolate", ["linear"], ["zoom"],
                    15, 1,
                    18, 11
                ],
                "line-translate": [
                    "interpolate", ["linear"], ["zoom"],
                    15, ["literal", [1, 1]],
                    18, ["literal", [4, 4]]
                ]
            }
        },
        {
            "id": "world1-building_fill",
            "minzoom": 15,
            "type": "fill",
            "source": "world1-buildings",
            "source-layer": "buildings",
            "paint": {
                "fill-pattern": "shade_light",
                "fill-antialias": false
            },
            "interactive": true
        },
        {
            "id": "world1-building_outline",
            "type": "line",
            "source": "world1-buildings",
            "source-layer": "buildings",
            "paint": {
                "line-pattern": "line_solid_6",
                "line-width": {
                    "base": 1.0,
                    "stops": [
                        [
                            11,
                            0.1
                        ],
                        [
                            18,
                            3
                        ]
                    ]
                }
            },
            "interactive": true
        },
        {
            "id": "world1-road_tertiary_bg",
            "type": "line",
            "source": "world1-roads",
            "source-layer": "roads",
            "interactive": true,
            "layout": {
                "line-cap": "round",
                "line-join": "round",
                "visibility": "visible"
            },
            "filter": [
                "all",
                [
                    "in",
                    "highway",
                    "tertiary"
                ],
                [
                    "!in",
                    "structure",
                    "bridge",
                    "tunnel"
                ]
            ],
            "paint": {
                "line-pattern": [
                    "step", ["zoom"],
                    "line_solid_6",
                    15, "line_double_14",
                    16, "line_double_16"
                ],
                "line-width": {
                    "base": 1.2,
                    "stops": [
                        [
                            11,
                            4
                        ],
                        [
                            20,
                            15
                        ]
                    ]
                },
                "line-opacity": [
                    "step", ["zoom"],
                    0.75,
                    14, 1.0
                ]
            }
        },
        {
            "id": "world1-road_secondary_bg",
            "type": "line",
            "source": "world1-roads",
            "source-layer": "roads",
            "interactive": true,
            "layout": {
                "visibility": "visible"
            },
            "filter": [
                "all",
                [
                    "in",
                    "highway",
                    "secondary"
                ],
                [
                    "!in",
                    "structure",
                    "bridge",
                    "tunnel"
                ]
            ],
            "paint": {
                "line-pattern": [
                    "step", ["zoom"],
                    "line_solid_6",
                    13, "line_solid_7",
                    14, "line_double_14",
                    15, "line_double_16",
                    18, "line_double_20"
                ],
                "line-width": {
                    "base": 1.2,
                    "stops": [
                        [
                            11,
                            6
                        ],
                        [
                            20,
                            17
                        ]
                    ]
                },
                "line-opacity": [
                    "step", ["zoom"],
                    0.75,
                    14, 1.0
                ]
            }
        },
        {
            "id": "world1-road_tertiary_fg",
            "type": "line",
            "source": "world1-roads",
            "source-layer": "roads",
            "minzoom": 15,
            "filter": [
                "all",
                [
                    "in",
                    "highway",
                    "tertiary"
                ],
                [
                    "!in",
                    "structure",
                    "bridge",
                    "tunnel"
                ]
            ],
            "paint": {
                "line-pattern": [
                    "step", ["zoom"],
                    "line_double_14_mask",
                    15, "line_double_14_mask",
                    16, "line_double_16_mask"
                ],
                "line-width": {
                    "base": 1.2,
                    "stops": [
                        [
                            11,
                            2
                        ],
                        [
                            20,
                            13
                        ]
                    ]
                }
            }
        },
        {
            "id": "world1-road_secondary_fg",
            "type": "line",
            "source": "world1-roads",
            "source-layer": "roads",
            "minzoom": 14,
            "layout": {
                "visibility": "visible"
            },
            "filter": [
                "all",
                [
                    "in",
                    "highway",
                    "secondary"
                ],
                [
                    "!in",
                    "structure",
                    "bridge",
                    "tunnel"
                ]
            ],
            "paint": {
                "line-pattern": [
                    "step", ["zoom"],
                    "line_double_14_mask",
                    14, "line_double_14_mask",
                    15, "line_double_16_mask",
                    18, "line_double_20_mask"
                ],
                "line-width": {
                    "base": 1.2,
                    "stops": [
                        [
                            11,
                            4
                        ],
                        [
                            20,
                            15
                        ]
                    ]
                }
            }
        },
        {
            "id": "world1-poi_label_3",
            "type": "symbol",
            "source": "world1-roads",
            "source-layer": "poi_label",
            "interactive": true,
            "minzoom": 15,
            "layout": {
                "icon-image": "marker-24",
                "text-font": [
                    "Open Sans Semibold"
                ],
                "text-field": "{name_en}",
                "text-max-width": 9,
                "text-padding": 2,
                "text-offset": [
                    0,
                    0.6
                ],
                "text-anchor": "top",
                "text-size": 12
            },
            "metadata": {
                "mapbox:group": "1444849297111.495"
            },
            "filter": [
                "all",
                [
                    "==",
                    "$type",
                    "Point"
                ],
                [
                    "==",
                    "scalerank",
                    3
                ]
            ],
            "paint": {
                "text-color": "#666",
                "text-halo-color": "#ffffff",
                "text-halo-width": 1,
                "text-halo-blur": 0.5
            }
        },
        {
            "id": "world1-road_label",
            "type": "symbol",
            "source": "world1-poi_label",
            "source-layer": "poi_label",
            "interactive": true,
            "minzoom": 15,
            "layout": {
                "text-font": [
                    "Open Sans Semibold"
                ],
                "text-field": "{name_en}",
                "text-max-width": 9,
                "text-padding": 2,
                "text-offset": [
                    0,
                    0
                ],
                "text-anchor": "center",
                "text-size": 12,
                "symbol-placement": "line-center"
            },
            "filter": [
                "all",
                [
                    "==",
                    "$type",
                    "LineString"
                ],
                [
                    "==",
                    "scalerank",
                    3
                ]
            ],
            "paint": {
                "text-color": "#666",
                "text-halo-color": "#ffffff",
                "text-halo-width": 1,
                "text-halo-blur": 0.5
            }
        },
        {
            "interactive": true,
            "layout": {
                "text-font": [
                    "Open Sans Regular"
                ],
                "text-field": "{name_en}",
                "text-max-width": 8,
                "text-size": {
                    "base": 1.2,
                    "stops": [
                        [
                            10,
                            14
                        ],
                        [
                            15,
                            24
                        ]
                    ]
                }
            },
            "filter": [
                "==",
                "place",
                "town"
            ],
            "type": "symbol",
            "source": "world1-place_label",
            "id": "world1-place_label_town",
            "paint": {
                "text-color": "#333",
                "text-halo-color": "rgba(255,255,255,0.8)",
                "text-halo-width": 1.2
            },
            "source-layer": "place_label"
        }
    ]
}
)JSON");

void MapboxGLWidget::initializeGL()
{
    m_map.reset(new QMapboxGL(nullptr, m_settings, size(), pixelRatio()));
    connect(m_map.data(), SIGNAL(needsRendering()), this, SLOT(update()));
    // Forward this signal
    connect(m_map.data(), &QMapboxGL::mapChanged, this, &MapboxGLWidget::mapChanged);

    m_map->setCoordinateZoom(QMapbox::Coordinate(-0.06042606042606043, 0.09841515314633154), 15);

#if 1
    try {
        m_map->setStyleJson(styleJson);
    } catch (const std::exception& ex) {
        qDebug() << ex.what();
    }
#else
    QString styleUrl(QStringLiteral("file://D:/pz/worktree/build40-weather/pzmapbox/data/pz-style.json"));
    m_map->setStyleUrl(styleUrl);
    setWindowTitle(QStringLiteral("Mapbox GL: ") + styleUrl);
#endif
}

void MapboxGLWidget::paintGL()
{
    m_map->resize(size());
    m_map->setFramebufferObject(defaultFramebufferObject(), size() * pixelRatio());
    m_map->render();
}

void MapboxGLWidget::setJson(const QMap<QString, QByteArray> &json)
{
    for (auto it = json.begin(); it != json.end(); it++) {
        QVariantMap source;
        source[QStringLiteral("type")] = QStringLiteral("geojson"); // The only supported type by QtMapbox
        source[QStringLiteral("data")] = it.value();
        qDebug() << it.key() << " string length = " << it.value().size();
        m_map->updateSource(QStringLiteral("world1-") + it.key(), source);
    }
}
