/*
 * Poly2Tri Copyright (c) 2009-2018, Poly2Tri Contributors
 * https://github.com/jhasse/poly2tri
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * * Neither the name of Poly2Tri nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without specific
 *   prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "sweep.h"
#include "sweep_context.h"
#include "advancing_front.h"
#include "../common/utils.h"

#include <cassert>
#include <stdexcept>

namespace p2t {

// Triangulate simple polygon with holes
void Sweep::Triangulate(SweepContext& tcx)
{
  tcx.InitTriangulation();
  tcx.CreateAdvancingFront();
  // Sweep points; build mesh
  SweepPoints(tcx);
  // Clean up
  FinalizationPolygon(tcx);
}

void Sweep::SweepPoints(SweepContext& tcx)
{
  for (size_t i = 1; i < tcx.point_count(); i++) {
    Point& point = *tcx.GetPoint(i);
    Node* node = &PointEvent(tcx, point);
    for (auto& j : point.edge_list) {
      EdgeEvent(tcx, j, node);
    }
  }
}

void Sweep::FinalizationPolygon(SweepContext& tcx)
{
  // Get an Internal triangle to start with
  Triangle* t = tcx.front()->head()->next->triangle;
  Point* p = tcx.front()->head()->next->point;
  while (t && !t->GetConstrainedEdgeCW(*p)) {
    t = t->NeighborCCW(*p);
  }

  // Collect interior triangles constrained by edges
  if (t) {
  tcx.MeshClean(*t);
}
}

Node& Sweep::PointEvent(SweepContext& tcx, Point& point)
{
  Node* node_ptr = tcx.LocateNode(point);
  if (!node_ptr || !node_ptr->point || !node_ptr->next || !node_ptr->next->point)
  {
    throw std::runtime_error("PointEvent - null node");
  }

  Node& node = *node_ptr;
  Node& new_node = NewFrontTriangle(tcx, point, node);

  // Only need to check +epsilon since point never have smaller
  // x value than node due to how we fetch nodes from the front
  if (point.x <= node.point->x + EPSILON) {
    Fill(tcx, node);
  }

  //tcx.AddNode(new_node);

  FillAdvancingFront(tcx, new_node);
  return new_node;
}

void Sweep::EdgeEvent(SweepContext& tcx, Edge* edge, Node* node)
{
  tcx.edge_event.constrained_edge = edge;
  tcx.edge_event.right = (edge->p->x > edge->q->x);

  if (IsEdgeSideOfTriangle(*node->triangle, *edge->p, *edge->q)) {
    return;
  }

  // For now we will do all needed filling
  // TODO: integrate with flip process might give some better performance
  //       but for now this avoid the issue with cases that needs both flips and fills
  FillEdgeEvent(tcx, edge, node);
  if( node->triangle )
  {
	  EdgeEvent( tcx, *edge->p, *edge->q, node->triangle, *edge->q );
  }
  else
  {
	  throw null_triangle_exception();
  }
}

void Sweep::EdgeEvent(SweepContext& tcx, Point& ep, Point& eq, Triangle* triangle, Point& point)
{
	GEOMETRIX_ASSERT( triangle );
	Triangle* prevTrig = triangle;
	if( triangle )
	{
		if( IsEdgeSideOfTriangle( *triangle, ep, eq ) )
		{
			return;
		}

		Point* p1 = triangle->PointCCW( point );
		if( p1 != nullptr )
		{
			Orientation o1 = Orient2d( eq, *p1, ep );
			if( o1 == COLLINEAR )
			{
				if( triangle->Contains( &eq, p1 ) )
				{
					triangle->MarkConstrainedEdge( &eq, p1 );
					// We are modifying the constraint maybe it would be better to
					// not change the given constraint and just keep a variable for the new constraint
					tcx.edge_event.constrained_edge->q = p1;
					triangle = triangle->NeighborAcross( point );
					if( triangle )
					{
						EdgeEvent( tcx, ep, *p1, triangle, *p1 );
					}
                    else
                    {
						throw null_triangle_exception( prevTrig );
                    }
				}
				else
				{
					// std::runtime_error("EdgeEvent - collinear points not supported");
					throw collinear_points_exception( eq, *p1, ep );
				}
				return;
			}
			Point* p2 = triangle->PointCW( point );
			if( p2 != nullptr )
			{
				Orientation o2 = Orient2d( eq, *p2, ep );
				if( o2 == COLLINEAR )
				{
					if( triangle->Contains( &eq, p2 ) )
					{
						triangle->MarkConstrainedEdge( &eq, p2 );
						// We are modifying the constraint maybe it would be better to
						// not change the given constraint and just keep a variable for the new constraint
						tcx.edge_event.constrained_edge->q = p2;
						triangle = triangle->NeighborAcross( point );
						if( triangle )
						{
							EdgeEvent( tcx, ep, *p2, triangle, *p2 );
						}
						else
						{
							throw null_triangle_exception( prevTrig );
						}
					}
					else
					{
						// std::runtime_error("EdgeEvent - collinear points not supported");
						throw collinear_points_exception( eq, *p2, ep );
					}
					return;
				}

				if( o1 == o2 )
				{
					// Need to decide if we are rotating CW or CCW to get to a triangle
					// that will cross edge
					if( o1 == CW )
					{
						triangle = triangle->NeighborCCW( point );
					}
					else
					{
						triangle = triangle->NeighborCW( point );
					}
					if( triangle )
					{
						EdgeEvent( tcx, ep, eq, triangle, point );
					}
					else
					{
						throw null_triangle_exception( prevTrig );
					}
				}
				else
				{
					// This triangle crosses constraint so lets flippin start!
    assert(triangle);
					FlipEdgeEvent( tcx, ep, eq, triangle, point );
				}
			}
			else
			{
				throw degenerate_triangle_exception{ *triangle->GetPoint( 0 ), *triangle->GetPoint( 1 ), *triangle->GetPoint( 2 ) };
			}
		}
		else
		{
			throw degenerate_triangle_exception{ *triangle->GetPoint( 0 ), *triangle->GetPoint( 1 ), *triangle->GetPoint( 2 ) };
		}
	}
	else
	{
		throw null_triangle_exception{ prevTrig };
	}
}

bool Sweep::IsEdgeSideOfTriangle(Triangle& triangle, Point& ep, Point& eq)
{
  const int index = triangle.EdgeIndex(&ep, &eq);

  if (index != -1) {
    triangle.MarkConstrainedEdge(index);
    Triangle* t = triangle.GetNeighbor(index);
    if (t) {
      t->MarkConstrainedEdge(&ep, &eq);
    }
    return true;
  }
  return false;
}

Node& Sweep::NewFrontTriangle(SweepContext& tcx, Point& point, Node& node)
{
  //GEOMETRIX_ASSERT(node.next);
  Triangle* triangle = new Triangle(point, *node.point, *node.next->point);

  triangle->MarkNeighbor(*node.triangle);
  tcx.AddToMap(triangle);

  Node* new_node = new Node(point);
  nodes_.push_back(new_node);

  new_node->next = node.next;
  new_node->prev = &node;
  node.next->prev = new_node;
  node.next = new_node;

  if (!Legalize(tcx, *triangle)) {
    tcx.MapTriangleToNodes(*triangle);
  }

  return *new_node;
}

void Sweep::Fill(SweepContext& tcx, Node& node)
{
  Triangle* triangle = new Triangle(*node.prev->point, *node.point, *node.next->point);

  // TODO: should copy the constrained_edge value from neighbor triangles
  //       for now constrained_edge values are copied during the legalize
  triangle->MarkNeighbor(*node.prev->triangle);
  triangle->MarkNeighbor(*node.triangle);

  tcx.AddToMap(triangle);

  // Update the advancing front
  node.prev->next = node.next;
  node.next->prev = node.prev;

  // If it was legalized the triangle has already been mapped
  if (!Legalize(tcx, *triangle)) {
    tcx.MapTriangleToNodes(*triangle);
  }
}

void Sweep::FillAdvancingFront(SweepContext& tcx, Node& n)
{

  // Fill right holes
  Node* node = n.next;

  while (node && node->next) {
    // if HoleAngle exceeds 90 degrees then break.
    if (LargeHole_DontFill(node)) break;
    Fill(tcx, *node);
    node = node->next;
  }

  // Fill left holes
  node = n.prev;

  while (node && node->prev) {
    // if HoleAngle exceeds 90 degrees then break.
    if (LargeHole_DontFill(node)) break;
    Fill(tcx, *node);
    node = node->prev;
  }

  // Fill right basins
  if (n.next && n.next->next) {
    const double angle = BasinAngle(n);
    if (angle < PI_3div4) {
      FillBasin(tcx, n);
    }
  }
}

// True if HoleAngle exceeds 90 degrees.
// LargeHole_DontFill checks if the advancing front has a large hole.
// A "Large hole" is a triangle formed by a sequence of points in the advancing
// front where three neighbor points form a triangle.
// And angle between left-top, bottom, and right-top points is more than 90 degrees.
// The first part of the algorithm reviews only three neighbor points, e.g. named A, B, C.
// Additional part of this logic reviews a sequence of 5 points -
// additionally reviews one point before and one after the sequence of three (A, B, C),
// e.g. named X and Y.
// In this case, angles are XBC and ABY and this if angles are negative or more
// than 90 degrees LargeHole_DontFill returns true.
// But there is a configuration when ABC has a negative angle but XBC or ABY is less
// than 90 degrees and positive.
// Then function LargeHole_DontFill return false and initiates filling.
// This filling creates a triangle ABC and adds it to the advancing front.
// But in the case when angle ABC is negative this triangle goes inside the advancing front
// and can intersect previously created triangles.
// This triangle leads to making wrong advancing front and problems in triangulation in the future.
// Looks like such a triangle should not be created.
// The simplest way to check and fix it is to check an angle ABC.
// If it is negative LargeHole_DontFill should return true and
// not initiate creating the ABC triangle in the advancing front.
// X______A         Y
//        \        /
//         \      /
//          \ B  /
//           |  /
//           | /
//           |/
//           C
bool Sweep::LargeHole_DontFill(const Node* node) const {

  const Node* nextNode = node->next;
  const Node* prevNode = node->prev;
  if (!AngleExceeds90Degrees(node->point, nextNode->point, prevNode->point))
          return false;

  if (AngleIsNegative(node->point, nextNode->point, prevNode->point))
          return true;

  // Check additional points on front.
  const Node* next2Node = nextNode->next;
  // "..Plus.." because only want angles on same side as point being added.
  if ((next2Node != nullptr) && !AngleExceedsPlus90DegreesOrIsNegative(node->point, next2Node->point, prevNode->point))
          return false;

  const Node* prev2Node = prevNode->prev;
  // "..Plus.." because only want angles on same side as point being added.
  if ((prev2Node != nullptr) && !AngleExceedsPlus90DegreesOrIsNegative(node->point, nextNode->point, prev2Node->point))
          return false;

  return true;
}

bool Sweep::AngleIsNegative(const Point* origin, const Point* pa, const Point* pb) const {
    const double angle = Angle(origin, pa, pb);
    return angle < 0;
}

bool Sweep::AngleExceeds90Degrees(const Point* origin, const Point* pa, const Point* pb) const {
  const double angle = Angle(origin, pa, pb);
  return ((angle > PI_div2) || (angle < -PI_div2));
}

bool Sweep::AngleExceedsPlus90DegreesOrIsNegative(const Point* origin, const Point* pa, const Point* pb) const {
  const double angle = Angle(origin, pa, pb);
  return (angle > PI_div2) || (angle < 0);
}

double Sweep::Angle(const Point* origin, const Point* pa, const Point* pb) const {
  /* Complex plane
   * ab = cosA +i*sinA
   * ab = (ax + ay*i)(bx + by*i) = (ax*bx + ay*by) + i(ax*by-ay*bx)
   * atan2(y,x) computes the principal value of the argument function
   * applied to the complex number x+iy
   * Where x = ax*bx + ay*by
   *       y = ax*by - ay*bx
   */
  const double px = origin->x;
  const double py = origin->y;
  const double ax = pa->x- px;
  const double ay = pa->y - py;
  const double bx = pb->x - px;
  const double by = pb->y - py;
  const double x = ax * by - ay * bx;
  const double y = ax * bx + ay * by;
  return atan2(x, y);
}

double Sweep::BasinAngle(const Node& node) const
{
  const double ax = node.point->x - node.next->next->point->x;
  const double ay = node.point->y - node.next->next->point->y;
  return atan2(ay, ax);
}

double Sweep::HoleAngle(const Node& node) const
{
  /* Complex plane
   * ab = cosA +i*sinA
   * ab = (ax + ay*i)(bx + by*i) = (ax*bx + ay*by) + i(ax*by-ay*bx)
   * atan2(y,x) computes the principal value of the argument function
   * applied to the complex number x+iy
   * Where x = ax*bx + ay*by
   *       y = ax*by - ay*bx
   */
  const double ax = node.next->point->x - node.point->x;
  const double ay = node.next->point->y - node.point->y;
  const double bx = node.prev->point->x - node.point->x;
  const double by = node.prev->point->y - node.point->y;
  return atan2(ax * by - ay * bx, ax * bx + ay * by);
}

bool Sweep::Legalize(SweepContext& tcx, Triangle& t)
{
  // To legalize a triangle we start by finding if any of the three edges
  // violate the Delaunay condition
  for (int i = 0; i < 3; i++) {
    if (t.delaunay_edge[i])
      continue;

    Triangle* ot = t.GetNeighbor(i);

    if (ot) {
      Point* p = t.GetPoint(i);
      Point* op = ot->OppositePoint(t, *p);
      int oi = ot->Index(op);

      // If this is a Constrained Edge or a Delaunay Edge(only during recursive legalization)
      // then we should not try to legalize
      if (ot->constrained_edge[oi] || ot->delaunay_edge[oi]) {
        t.constrained_edge[i] = ot->constrained_edge[oi];
        continue;
      }

      bool inside = Incircle(*p, *t.PointCCW(*p), *t.PointCW(*p), *op);

      if (inside) {
        // Lets mark this shared edge as Delaunay
        t.delaunay_edge[i] = true;
        ot->delaunay_edge[oi] = true;

        // Lets rotate shared edge one vertex CW to legalize it
        RotateTrianglePair(t, *p, *ot, *op);

        // We now got one valid Delaunay Edge shared by two triangles
        // This gives us 4 new edges to check for Delaunay

        // Make sure that triangle to node mapping is done only one time for a specific triangle
        bool not_legalized = !Legalize(tcx, t);
        if (not_legalized) {
          tcx.MapTriangleToNodes(t);
        }

        not_legalized = !Legalize(tcx, *ot);
        if (not_legalized)
          tcx.MapTriangleToNodes(*ot);

        // Reset the Delaunay edges, since they only are valid Delaunay edges
        // until we add a new triangle or point.
        // XXX: need to think about this. Can these edges be tried after we
        //      return to previous recursive level?
        t.delaunay_edge[i] = false;
        ot->delaunay_edge[oi] = false;

        // If triangle have been legalized no need to check the other edges since
        // the recursive legalization will handles those so we can end here.
        return true;
      }
    }
  }
  return false;
}

namespace {
	bool IncircleOld(const Point& pa, const Point& pb, const Point& pc, const Point& pd)
	{
	  const double adx = pa.x - pd.x;
	  const double ady = pa.y - pd.y;
	  const double bdx = pb.x - pd.x;
	  const double bdy = pb.y - pd.y;

	  const double adxbdy = adx * bdy;
	  const double bdxady = bdx * ady;
	  const double oabd = adxbdy - bdxady;

	  if (oabd <= 0)
		return false;

	  const double cdx = pc.x - pd.x;
	  const double cdy = pc.y - pd.y;

	  const double cdxady = cdx * ady;
	  const double adxcdy = adx * cdy;
	  const double ocad = cdxady - adxcdy;

	  if (ocad <= 0)
		return false;

	  const double bdxcdy = bdx * cdy;
	  const double cdxbdy = cdx * bdy;

	  const double alift = adx * adx + ady * ady;
	  const double blift = bdx * bdx + bdy * bdy;
	  const double clift = cdx * cdx + cdy * cdy;

	  const double det = alift * (bdxcdy - cdxbdy) + blift * ocad + clift * oabd;

	  return det > 0;
	}
}
bool Sweep::Incircle(const Point& pa, const Point& pb, const Point& pc, const Point& pd) const
{
#ifndef POLY2TRI_USE_EXACT
  const double adx = pa.x - pd.x;
  const double ady = pa.y - pd.y;
  const double bdx = pb.x - pd.x;
  const double bdy = pb.y - pd.y;

  const double adxbdy = adx * bdy;
  const double bdxady = bdx * ady;
  const double oabd = adxbdy - bdxady;

  if (oabd <= 0)
    return false;

  const double cdx = pc.x - pd.x;
  const double cdy = pc.y - pd.y;

  const double cdxady = cdx * ady;
  const double adxcdy = adx * cdy;
  const double ocad = cdxady - adxcdy;

  if (ocad <= 0)
    return false;

  const double bdxcdy = bdx * cdy;
  const double cdxbdy = cdx * bdy;

  const double alift = adx * adx + ady * ady;
  const double blift = bdx * bdx + bdy * bdy;
  const double clift = cdx * cdx + cdy * cdy;

  const double det = alift * (bdxcdy - cdxbdy) + blift * ocad + clift * oabd;

  return det > 0;
#else
	std::array<double, 2> a = { pa.x, pa.y };
	std::array<double, 2> b = { pb.x, pb.y };
	std::array<double, 2> c = { pc.x, pc.y };
	std::array<double, 2> d = { pd.x, pd.y };
	auto r = exact::in_circumcircle(a, b, c, d) != geometrix::oriented_right;

//#ifndef NDEBUG
//	std::array<geometrix::point<double, 2>, 3> trig = { geometrix::point<double,2>{a[0], a[1]}, geometrix::point<double,2>{b[0], b[1]}, geometrix::point<double,2>{c[0], c[1]} };
//	geometrix::point<double, 2> gd(d[0], d[1]);
	//GEOMETRIX_ASSERT(r == IncircleOld(pa, pb, pc, pd));
//#endif

	return r;
#endif
}

void Sweep::RotateTrianglePair(Triangle& t, Point& p, Triangle& ot, Point& op) const
{
  Triangle* n1, *n2, *n3, *n4;
  n1 = t.NeighborCCW(p);
  n2 = t.NeighborCW(p);
  n3 = ot.NeighborCCW(op);
  n4 = ot.NeighborCW(op);

  bool ce1, ce2, ce3, ce4;
  ce1 = t.GetConstrainedEdgeCCW(p);
  ce2 = t.GetConstrainedEdgeCW(p);
  ce3 = ot.GetConstrainedEdgeCCW(op);
  ce4 = ot.GetConstrainedEdgeCW(op);

  bool de1, de2, de3, de4;
  de1 = t.GetDelunayEdgeCCW(p);
  de2 = t.GetDelunayEdgeCW(p);
  de3 = ot.GetDelunayEdgeCCW(op);
  de4 = ot.GetDelunayEdgeCW(op);

  t.Legalize(p, op);
  ot.Legalize(op, p);

  // Remap delaunay_edge
  ot.SetDelunayEdgeCCW(p, de1);
  t.SetDelunayEdgeCW(p, de2);
  t.SetDelunayEdgeCCW(op, de3);
  ot.SetDelunayEdgeCW(op, de4);

  // Remap constrained_edge
  ot.SetConstrainedEdgeCCW(p, ce1);
  t.SetConstrainedEdgeCW(p, ce2);
  t.SetConstrainedEdgeCCW(op, ce3);
  ot.SetConstrainedEdgeCW(op, ce4);

  // Remap neighbors
  // XXX: might optimize the markNeighbor by keeping track of
  //      what side should be assigned to what neighbor after the
  //      rotation. Now mark neighbor does lots of testing to find
  //      the right side.
  t.ClearNeighbors();
  ot.ClearNeighbors();
  if (n1) ot.MarkNeighbor(*n1);
  if (n2) t.MarkNeighbor(*n2);
  if (n3) t.MarkNeighbor(*n3);
  if (n4) ot.MarkNeighbor(*n4);
  t.MarkNeighbor(ot);
}

void Sweep::FillBasin(SweepContext& tcx, Node& node)
{
  if (Orient2d(*node.point, *node.next->point, *node.next->next->point) == CCW) {
    tcx.basin.left_node = node.next->next;
  } else {
    tcx.basin.left_node = node.next;
  }

  // Find the bottom and right node
  tcx.basin.bottom_node = tcx.basin.left_node;
  while (tcx.basin.bottom_node->next
         && tcx.basin.bottom_node->point->y >= tcx.basin.bottom_node->next->point->y) {
    tcx.basin.bottom_node = tcx.basin.bottom_node->next;
  }
  if (tcx.basin.bottom_node == tcx.basin.left_node) {
    // No valid basin
    return;
  }

  tcx.basin.right_node = tcx.basin.bottom_node;
  while (tcx.basin.right_node->next
         && tcx.basin.right_node->point->y < tcx.basin.right_node->next->point->y) {
    tcx.basin.right_node = tcx.basin.right_node->next;
  }
  if (tcx.basin.right_node == tcx.basin.bottom_node) {
    // No valid basins
    return;
  }

  tcx.basin.width = tcx.basin.right_node->point->x - tcx.basin.left_node->point->x;
  tcx.basin.left_highest = tcx.basin.left_node->point->y > tcx.basin.right_node->point->y;

  FillBasinReq(tcx, tcx.basin.bottom_node);
}

void Sweep::FillBasinReq(SweepContext& tcx, Node* node)
{
  // if shallow stop filling
  if (IsShallow(tcx, *node)) {
    return;
  }

  Fill(tcx, *node);

  if (node->prev == tcx.basin.left_node && node->next == tcx.basin.right_node) {
    return;
  } else if (node->prev == tcx.basin.left_node) {
    Orientation o = Orient2d(*node->point, *node->next->point, *node->next->next->point);
    if (o == CW) {
      return;
    }
    node = node->next;
  } else if (node->next == tcx.basin.right_node) {
    Orientation o = Orient2d(*node->point, *node->prev->point, *node->prev->prev->point);
    if (o == CCW) {
      return;
    }
    node = node->prev;
  } else {
    // Continue with the neighbor node with lowest Y value
    if (node->prev->point->y < node->next->point->y) {
      node = node->prev;
    } else {
      node = node->next;
    }
  }

  FillBasinReq(tcx, node);
}

bool Sweep::IsShallow(SweepContext& tcx, Node& node)
{
  double height;

  if (tcx.basin.left_highest) {
    height = tcx.basin.left_node->point->y - node.point->y;
  } else {
    height = tcx.basin.right_node->point->y - node.point->y;
  }

  // if shallow stop filling
  if (tcx.basin.width > height) {
    return true;
  }
  return false;
}

void Sweep::FillEdgeEvent(SweepContext& tcx, Edge* edge, Node* node)
{
  if (tcx.edge_event.right) {
    FillRightAboveEdgeEvent(tcx, edge, node);
  } else {
    FillLeftAboveEdgeEvent(tcx, edge, node);
  }
}

void Sweep::FillRightAboveEdgeEvent(SweepContext& tcx, Edge* edge, Node* node)
{
  while (node->next->point->x < edge->p->x) {
    // Check if next node is below the edge
    if (Orient2d(*edge->q, *node->next->point, *edge->p) == CCW) {
      FillRightBelowEdgeEvent(tcx, edge, *node);
    } else {
      node = node->next;
    }
  }
}

void Sweep::FillRightBelowEdgeEvent(SweepContext& tcx, Edge* edge, Node& node)
{
  if (node.point->x < edge->p->x) {
    if (Orient2d(*node.point, *node.next->point, *node.next->next->point) == CCW) {
      // Concave
      FillRightConcaveEdgeEvent(tcx, edge, node);
    } else{
      // Convex
      FillRightConvexEdgeEvent(tcx, edge, node);
      // Retry this one
      FillRightBelowEdgeEvent(tcx, edge, node);
    }
  }
}

void Sweep::FillRightConcaveEdgeEvent(SweepContext& tcx, Edge* edge, Node& node)
{
  Fill(tcx, *node.next);
  if (node.next->point != edge->p) {
    // Next above or below edge?
    if (Orient2d(*edge->q, *node.next->point, *edge->p) == CCW) {
      // Below
      if (Orient2d(*node.point, *node.next->point, *node.next->next->point) == CCW) {
        // Next is concave
        FillRightConcaveEdgeEvent(tcx, edge, node);
      } else {
        // Next is convex
      }
    }
  }
}

void Sweep::FillRightConvexEdgeEvent(SweepContext& tcx, Edge* edge, Node& node)
{
  // Next concave or convex?
  if (Orient2d(*node.next->point, *node.next->next->point, *node.next->next->next->point) == CCW) {
    // Concave
    FillRightConcaveEdgeEvent(tcx, edge, *node.next);
  } else{
    // Convex
    // Next above or below edge?
    if (Orient2d(*edge->q, *node.next->next->point, *edge->p) == CCW) {
      // Below
      FillRightConvexEdgeEvent(tcx, edge, *node.next);
    } else{
      // Above
    }
  }
}

void Sweep::FillLeftAboveEdgeEvent(SweepContext& tcx, Edge* edge, Node* node)
{
  while (node->prev->point->x > edge->p->x) {
    // Check if next node is below the edge
    if (Orient2d(*edge->q, *node->prev->point, *edge->p) == CW) {
      FillLeftBelowEdgeEvent(tcx, edge, *node);
    } else {
      node = node->prev;
    }
  }
}

void Sweep::FillLeftBelowEdgeEvent(SweepContext& tcx, Edge* edge, Node& node)
{
  if (node.point->x > edge->p->x) {
    if (Orient2d(*node.point, *node.prev->point, *node.prev->prev->point) == CW) {
      // Concave
      FillLeftConcaveEdgeEvent(tcx, edge, node);
    } else {
      // Convex
      FillLeftConvexEdgeEvent(tcx, edge, node);
      // Retry this one
      FillLeftBelowEdgeEvent(tcx, edge, node);
    }
  }
}

void Sweep::FillLeftConvexEdgeEvent(SweepContext& tcx, Edge* edge, Node& node)
{
  // Next concave or convex?
  if (Orient2d(*node.prev->point, *node.prev->prev->point, *node.prev->prev->prev->point) == CW) {
    // Concave
    FillLeftConcaveEdgeEvent(tcx, edge, *node.prev);
  } else{
    // Convex
    // Next above or below edge?
    if (Orient2d(*edge->q, *node.prev->prev->point, *edge->p) == CW) {
      // Below
      FillLeftConvexEdgeEvent(tcx, edge, *node.prev);
    } else{
      // Above
    }
  }
}

void Sweep::FillLeftConcaveEdgeEvent(SweepContext& tcx, Edge* edge, Node& node)
{
  Fill(tcx, *node.prev);
  if (node.prev->point != edge->p) {
    // Next above or below edge?
    if (Orient2d(*edge->q, *node.prev->point, *edge->p) == CW) {
      // Below
      if (Orient2d(*node.point, *node.prev->point, *node.prev->prev->point) == CW) {
        // Next is concave
        FillLeftConcaveEdgeEvent(tcx, edge, node);
      } else{
        // Next is convex
      }
    }
  }
}

void Sweep::FlipEdgeEvent(SweepContext& tcx, Point& ep, Point& eq, Triangle* t, Point& p)
{
  assert(t);
  Triangle* ot_ptr = t->NeighborAcross(p);
  if (ot_ptr == nullptr)
  {
    throw std::runtime_error("FlipEdgeEvent - null neighbor across");
  }
  Triangle& ot = *ot_ptr;
  Point& op = *ot.OppositePoint(*t, p);

  if (InScanArea(p, *t->PointCCW(p), *t->PointCW(p), op)) {
    // Lets rotate shared edge one vertex CW
    RotateTrianglePair(*t, p, ot, op);
    tcx.MapTriangleToNodes(*t);
    tcx.MapTriangleToNodes(ot);

    if (p == eq && op == ep) {
      if (eq == *tcx.edge_event.constrained_edge->q && ep == *tcx.edge_event.constrained_edge->p) {
        t->MarkConstrainedEdge(&ep, &eq);
        ot.MarkConstrainedEdge(&ep, &eq);
        Legalize(tcx, *t);
        Legalize(tcx, ot);
      } else {
        // XXX: I think one of the triangles should be legalized here?
      }
    } else {
      Orientation o = Orient2d(eq, op, ep);
      t = &NextFlipTriangle(tcx, o, *t, ot, p, op);
      FlipEdgeEvent(tcx, ep, eq, t, p);
    }
  } else {
    Point& newP = NextFlipPoint(ep, eq, ot, op);
    FlipScanEdgeEvent(tcx, ep, eq, *t, ot, newP);
    EdgeEvent(tcx, ep, eq, t, p);
  }
}

Triangle& Sweep::NextFlipTriangle(SweepContext& tcx, Orientation o, Triangle& t, Triangle& ot, Point& p, Point& op)
{
  if (o == CCW) {
    // ot is not crossing edge after flip
    int edge_index = ot.EdgeIndex(&p, &op);
    ot.delaunay_edge[edge_index] = true;
    Legalize(tcx, ot);
    ot.ClearDelunayEdges();
    return t;
  }

  // t is not crossing edge after flip
  int edge_index = t.EdgeIndex(&p, &op);

  t.delaunay_edge[edge_index] = true;
  Legalize(tcx, t);
  t.ClearDelunayEdges();
  return ot;
}

Point& Sweep::NextFlipPoint(Point& ep, Point& eq, Triangle& ot, Point& op)
{
  Orientation o2d = Orient2d(eq, op, ep);
  if (o2d == CW) {
    // Right
    return *ot.PointCCW(op);
  } else if (o2d == CCW) {
    // Left
    return *ot.PointCW(op);
  }
  throw collinear_points_exception(eq, op, ep);// std::runtime_error("[Unsupported] Opposing point on constrained edge");
}

void Sweep::FlipScanEdgeEvent(SweepContext& tcx, Point& ep, Point& eq, Triangle& flip_triangle,
                              Triangle& t, Point& p)
{
  Triangle* ot_ptr = t.NeighborAcross(p);
  if (ot_ptr == nullptr) {
    throw std::runtime_error("FlipScanEdgeEvent - null neighbor across");
  }

  Point* op_ptr = ot_ptr->OppositePoint(t, p);
  if (op_ptr == nullptr) {
    throw std::runtime_error("FlipScanEdgeEvent - null opposing point");
  }

  Point* p1 = flip_triangle.PointCCW(eq);
  Point* p2 = flip_triangle.PointCW(eq);
  if (p1 == nullptr || p2 == nullptr) {
    throw std::runtime_error("FlipScanEdgeEvent - null on either of points");
  }

  Triangle& ot = *ot_ptr;
  Point& op = *op_ptr;

  if (InScanArea(eq, *p1, *p2, op)) {
    // flip with new edge op->eq
    FlipEdgeEvent(tcx, eq, op, &ot, op);
    // TODO: Actually I just figured out that it should be possible to
    //       improve this by getting the next ot and op before the the above
    //       flip and continue the flipScanEdgeEvent here
    // set new ot and op here and loop back to inScanArea test
    // also need to set a new flip_triangle first
    // Turns out at first glance that this is somewhat complicated
    // so it will have to wait.
  } else{
    Point& newP = NextFlipPoint(ep, eq, ot, op);
    FlipScanEdgeEvent(tcx, ep, eq, flip_triangle, ot, newP);
  }
}

Sweep::~Sweep() {
    // Clean up memory
    for (auto& node : nodes_) {
      delete node;
    }
}

}//! namespace pt2;

