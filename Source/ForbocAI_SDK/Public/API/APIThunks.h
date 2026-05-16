#pragma once

#include "Core/ThunkDetail.h"

namespace rtk {

/**
 * Builds the thunk that checks API status for CLI doctor flows.
 * User Story: As runtime health checks, I need a lightweight doctor thunk so
 * callers can verify API availability through the store contract.
 */
inline ThunkAction<FApiStatusResponse, FStoreState> doctorThunk() {
  return [](std::function<AnyAction(const AnyAction &)> Dispatch,
            std::function<FStoreState()> GetState)
             -> func::AsyncResult<FApiStatusResponse> {
    return APISlice::Endpoints::getApiStatus()(Dispatch, GetState);
  };
}

/**
 * Builds the thunk alias for API status checks.
 * User Story: As callers using clearer naming, I need a semantic alias so
 * health checks can be invoked without duplicating implementation.
 */
inline ThunkAction<FApiStatusResponse, FStoreState> checkApiStatusThunk() {
  return doctorThunk();
}

} // namespace rtk
