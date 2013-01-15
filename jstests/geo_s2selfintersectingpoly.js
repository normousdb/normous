t = db.geo_s2selfintersectingpoly;
t.drop();
t.ensureIndex({geo: "2dsphere"});

intersectingPolygon = {"type": "Polygon", "coordinates": [
    [[0.0, 0.0], [0.0, 4.0], [-3.0, 2.0], [1.0, 2.0], [0.0, 0.0]]
]};
/*
 * Self intersecting polygons should cause a parse exception.
 */
t.insert({geo : intersectingPolygon});
assert(db.getLastError());
