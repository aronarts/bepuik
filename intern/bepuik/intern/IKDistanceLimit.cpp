/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2013 by
 * Harrison Nordby and Ross Nordby
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "IKDistanceLimit.hpp"
#include "Toolbox.hpp"
#include <cmath>

namespace BEPUik
{

	/// <summary>
	/// Gets the offset in world space from the center of mass of connection A to the anchor point.
	/// </summary>
	Vector3 IKDistanceLimit::GetAnchorA()
	{
		Vector3 toReturn;
		Quaternion::Transform(LocalAnchorA, connectionA->Orientation, toReturn);
		Vector3::Add(connectionA->Position, toReturn, toReturn);
		return toReturn;
	}

	/// <summary>
	/// Sets the offset in world space from the center of mass of connection A to the anchor point.
	/// </summary>
	void IKDistanceLimit::SetAnchorA(Vector3 anchor)
	{
		Vector3::Subtract(anchor, connectionA->Position, anchor);
		Quaternion conjugate;
		Quaternion::Conjugate(connectionA->Orientation, conjugate);
		Quaternion::Transform(anchor, conjugate, anchor);
		LocalAnchorA = anchor;
	}

	/// <summary>
	/// Gets the offset in world space from the center of mass of connection B to the anchor point.
	/// </summary>
	Vector3 IKDistanceLimit::GetAnchorB()
	{
		Vector3 toReturn;
		Quaternion::Transform(LocalAnchorB, connectionB->Orientation, toReturn);
		Vector3::Add(connectionB->Position, toReturn, toReturn);
		return toReturn;
	}

	/// <summary>
	/// Sets the offset in world space from the center of mass of connection B to the anchor point.
	/// </summary>
	void IKDistanceLimit::SetAnchorB(Vector3 anchor)
	{
		Vector3::Subtract(anchor, connectionB->Position, anchor);
		Quaternion conjugate;
		Quaternion::Conjugate(connectionB->Orientation, conjugate);
		Quaternion::Transform(anchor, conjugate, anchor);
		LocalAnchorB = anchor;
	}

	/// <summary>
	/// Gets the distance that the joint connections should be kept from each other.
	/// </summary>
	float IKDistanceLimit::GetMinimumDistance()
	{
		return minimumDistance;
	}
	/// <summary>
	/// Sets the distance that the joint connections should be kept from each other.
	/// </summary>
	void IKDistanceLimit::SetMinimumDistance(float newDistance)
	{
		minimumDistance = MathHelper::Max(0, newDistance);
	}

	/// <summary>
	/// Gets the distance that the joint connections should be kept from each other.
	/// </summary>
	float IKDistanceLimit::GetMaximumDistance()
	{
		return maximumDistance;
	}
	/// <summary>
	/// Sets the distance that the joint connections should be kept from each other.
	/// </summary>
	void IKDistanceLimit::SetMaximumDistance(float newDistance)
	{
		maximumDistance = MathHelper::Max(0, newDistance);
	}


	/// <summary>
	/// Constructs a new distance joint.
	/// </summary>
	/// <param name="connectionA">First bone connected by the joint.</param>
	/// <param name="connectionB">Second bone connected by the joint.</param>
	/// <param name="anchorA">Anchor point on the first bone in world space.</param>
	/// <param name="anchorB">Anchor point on the second bone in world space.</param>
	/// <param name="minimumDistance">Minimum distance that the joint connections should be kept from each other.</param>
	/// <param name="maximumDistance">Maximum distance that the joint connections should be kept from each other.</param>
	IKDistanceLimit::IKDistanceLimit(IKBone* connectionA, IKBone* connectionB, Vector3 anchorA, Vector3 anchorB, float minimumDistance, float maximumDistance)
		: IKLimit(connectionA, connectionB)
	{
		SetAnchorA(anchorA);
		SetAnchorB(anchorB);
		SetMinimumDistance(minimumDistance);
		SetMaximumDistance(maximumDistance);
	}

	void IKDistanceLimit::UpdateJacobiansAndVelocityBias()
	{
		//Transform the anchors and offsets into world space.
		Vector3 offsetA, offsetB;
		Quaternion::Transform(LocalAnchorA, connectionA->Orientation, offsetA);
		Quaternion::Transform(LocalAnchorB, connectionB->Orientation, offsetB);
		Vector3 anchorA, anchorB;
		Vector3::Add(connectionA->Position, offsetA, anchorA);
		Vector3::Add(connectionB->Position, offsetB, anchorB);

		//Compute the distance.
		Vector3 separation;
		Vector3::Subtract(anchorB, anchorA, separation);
		float currentDistance = separation.Length();

		//Compute jacobians
		Vector3 linearA;
		if (currentDistance > Toolbox::Epsilon)
		{
			linearA.X = separation.X / currentDistance;
			linearA.Y = separation.Y / currentDistance;
			linearA.Z = separation.Z / currentDistance;

			if (currentDistance > maximumDistance)
			{
				//We are exceeding the maximum limit.
				error = (currentDistance - maximumDistance);
				velocityBias = Vector3(errorCorrectionFactor * error, 0, 0);
			}
			else if (currentDistance < minimumDistance)
			{
				//We are exceeding the minimum limit.
				error = (minimumDistance - currentDistance);
				velocityBias = Vector3(errorCorrectionFactor * error, 0, 0);
				//The limit can only push in one direction. Flip the jacobian!
				Vector3::Negate(linearA, linearA);
			}
			else if (currentDistance - minimumDistance > (maximumDistance - minimumDistance) * 0.5f)
			{
				//The objects are closer to hitting the maximum limit.
				error = 0;
				velocityBias = Vector3(currentDistance - maximumDistance, 0, 0);
			}
			else
			{
				//The objects are closer to hitting the minimum limit.
				error = 0;
				velocityBias = Vector3(minimumDistance - currentDistance, 0, 0);
				//The limit can only push in one direction. Flip the jacobian!
				Vector3::Negate(linearA, linearA);
			}
		}
		else
		{
			error = 0;
			velocityBias = Vector3();
			linearA = Vector3();
		}

		Vector3 angularA, angularB;
		Vector3::Cross(offsetA, linearA, angularA);
		//linearB = -linearA, so just swap the cross product order.
		Vector3::Cross(linearA, offsetB, angularB);

		//Put all the 1x3 jacobians into a 3x3 matrix representation.
		linearJacobianA = Matrix3X3();
		linearJacobianA.M11 = linearA.X;
		linearJacobianA.M12 = linearA.Y;
		linearJacobianA.M13 = linearA.Z;
		linearJacobianB = Matrix3X3();
		linearJacobianB.M11 = -linearA.X;
		linearJacobianB.M12 = -linearA.Y;
		linearJacobianB.M13 = -linearA.Z;
		angularJacobianA = Matrix3X3();
		angularJacobianA.M11 = angularA.X;
		angularJacobianA.M12 = angularA.Y;
		angularJacobianA.M13 = angularA.Z;
		angularJacobianB = Matrix3X3();
		angularJacobianB.M11 = angularB.X;
		angularJacobianB.M12 = angularB.Y;
		angularJacobianB.M13 = angularB.Z;

	}
	
	bool IKDistanceLimit::HasError()
	{
		return !(std::abs(error) < 0.0001f);
	}
}
