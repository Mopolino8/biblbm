#ifndef SHEAR_FLOW_SETUP_H_
#define SHEAR_FLOW_SETUP_H_

////// ________________________________________________________________
template<class T>
T simpleShearVelocity(plb::plint iY, plb::plint Ny, T U) {
	const T b = Ny-1;
	const T y = 2.0 * (T)iY / b - 1;
	return U * y;
}

////// ________________________________________________________________
template <typename T>
class SimpleShearVelocity {
public:
	SimpleShearVelocity(plb::plint margin_y_, plb::plint Ny_, T U_)
	: Ny(Ny_), U(U_), margin_y(margin_y_)
	{ }
	void operator()(plb::plint iX, plb::plint iY, plb::plint iZ, plb::Array<T,3>& u) const  {
		u[0] = simpleShearVelocity(iY-margin_y, Ny-2*margin_y, U);
		u[1] = T();
		u[2] = T();
	}
private:
	plb::plint Ny;
	plb::plint margin_y;
	T U;
};

////// ________________________________________________________________
template <typename T>
class SimpleShearDensityAndVelocity {
public:
	SimpleShearDensityAndVelocity(plb::plint margin_y_, plb::plint Ny_, T U_)
	: Ny(Ny_), U(U_), margin_y(margin_y_)
	{ }
	void operator()(plb::plint iX, plb::plint iY, plb::plint iZ, T &rho, plb::Array<T,3>& u) const {
		rho = (T)1;
		u[0] = simpleShearVelocity(iY-margin_y, Ny-2*margin_y, U);
		u[1] = T();
		u[2] = T();
	}
private:
	T U;
	plb::plint Ny;
	plb::plint margin_y;
};

////// ________________________________________________________________
template<class T, template<typename U> class Descriptor>
void shearFlowSetup( plb::MultiBlockLattice3D<T,Descriptor>& lattice,
		T U,
		plb::OnLatticeBoundaryCondition3D<T,Descriptor>& boundaryCondition,
		plb::plint margin_y)
{
	using namespace plb;

	const plint nx = lattice.getBoundingBox().getNx();
	const plint ny = lattice.getBoundingBox().getNy();
	const plint nz = lattice.getBoundingBox().getNz();
	const plint geoNx = nx-1;
	const plint geoNy = ny-1;
	const plint geoNz = nz-1;

	//plb::Box3D leftSideSurface = plb::Box3D(0, 0, 1, ny-2, 0, nz-1);
	//plb::Box3D rightSideSurface = plb::Box3D(nx-1, nx-1, 1, ny-2, 0, nz-1);
	plb::Box3D topSurface = plb::Box3D(0, geoNx, geoNy-margin_y, geoNy-margin_y, 0, geoNz);
	plb::Box3D bottomSurface = plb::Box3D(0, geoNx, margin_y, margin_y, 0, geoNz);
	//plb::Box3D frontSideSurface = plb::Box3D(1, nx-2, 1, ny-2, 0, 0);
	//plb::Box3D backSideSurface = plb::Box3D(1, nx-2, 1, ny-2, nz-1, nz-1);
	//plb::Box3D inside = plb::Box3D(0, nx-1, 1, ny-2, 0, nz-1);

	lattice.periodicity().toggle(0, true); // Use periodic boundaries on X direction.
	lattice.periodicity().toggle(2, true); // Use periodic boundaries on Z direction.

	// Set noDynamics outside the domain
	if(margin_y > 0) {
		Box3D topOuter(0, geoNx, geoNy-margin_y+1, geoNy, 0, geoNz);
		Box3D bottomOuter(0, geoNx, 0, margin_y-1, 0, geoNz);
		defineDynamics<double,Descriptor>(lattice,
					topOuter,
					new plb::NoDynamics<T, Descriptor>() );
		defineDynamics<double,Descriptor>(lattice,
					bottomOuter,
					new plb::NoDynamics<T, Descriptor>() );
	}

	boundaryCondition.addVelocityBoundary1N(bottomSurface, lattice);
	boundaryCondition.addVelocityBoundary1P(topSurface,    lattice);

	initializeAtEquilibrium(lattice, lattice.getBoundingBox(),
			SimpleShearDensityAndVelocity<T>(margin_y, ny, U) );

	setBoundaryVelocity(lattice, lattice.getBoundingBox(), SimpleShearVelocity<T>(margin_y, ny, U) );

	lattice.initialize();
}

template<class T, template<typename U> class Descriptor>
void shearFlowSetup( plb::MultiBlockLattice3D<T,Descriptor>& lattice,
		T U,
		plb::OnLatticeBoundaryCondition3D<T,Descriptor>& boundaryCondition)
{
	shearFlowSetup(lattice, U, boundaryCondition, 0);
}

/**** Velocity extrapolation ****/
template<class T>
class CouetteVelocityExtrapolator : public plb::BoxProcessingFunctional3D_T<T, 3>
{
public:
	CouetteVelocityExtrapolator(plb::plint geoNy, plb::plint margin_y, T uMax)
	: geoNy_(geoNy), margin_y_(margin_y), uMax_(uMax)
	{ }

	void process(plb::Box3D domain, plb::TensorField3D<T, 3> & velocity)
	{
		using namespace plb;

		if(margin_y_ < 1)
			return;

		Dot3D offset = velocity.getLocation();
		Box3D global_domain = domain.shift(offset.x, offset.y, offset.z);

		if(margin_y_ == 1) {
			// Lower boundary
			if(global_domain.y0 == 0) {
				plint iY = domain.y0;
				for(plint iX = domain.x0; iX <= domain.x1; ++iX) {
					for(plint iZ = domain.z0; iZ <= domain.z1; ++iZ) {
						velocity.get(iX, iY, iZ) = 2.0*velocity.get(iX, iY+1, iZ) - velocity.get(iX, iY+2, iZ);
					}
				}
			}

			// Upper boundary
			if(global_domain.y1 == (geoNy_+2*margin_y_)) {
				plint iY = domain.y1;
				for(plint iX = domain.x0; iX <= domain.x1; ++iX) {
					for(plint iZ = domain.z0; iZ <= domain.z1; ++iZ) {
						velocity.get(iX, iY, iZ) = 2.0*velocity.get(iX, iY-1, iZ) - velocity.get(iX, iY-2, iZ);
					}
				}
			}

		} else if(margin_y_ == 2) {

		} else {
			std::cerr << "CouetteVelocityExtrapolator::process: Cannot handle domains with margin > 2" << std::endl;
		}
	}


	virtual CouetteVelocityExtrapolator * clone() const
	{
		return new CouetteVelocityExtrapolator(*this);
	}

	virtual plb::BlockDomain::DomainT appliesTo() const {
		return plb::BlockDomain::bulk;
	}

	virtual void getModificationPattern(std::vector<bool>& isWritten) const {
		isWritten[0] = false;
	}

	virtual void getTypeOfModification(std::vector<plb::modif::ModifT> & modified) const {
		modified[0] = plb::modif::staticVariables;
	}

private:
	plb::plint geoNy_, margin_y_;
	T uMax_;
};

#endif /* SHEAR_FLOW_SETUP_H_ */
