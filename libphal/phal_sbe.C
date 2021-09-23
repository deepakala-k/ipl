#include "libphal.H"
#include "log.H"
#include "phal_exception.H"
#include "utils_buffer.H"
#include "utils_pdbg.H"
#include "utils_tempfile.H"

extern "C" {
#include <libpdbg_sbe.h>
}

namespace openpower::phal
{
namespace sbe
{

using namespace openpower::phal::logging;
using namespace openpower::phal;
using namespace openpower::phal::utils::pdbg;

void validateSBEState(struct pdbg_target *proc)
{
	// In order to allow SBE operation following conditions should be met
	// 1. Caller has to make sure processor target should be functional
	// or dump functional
	// 2. Processor's SBE state should be marked as BOOTED or CHECK_CFAM
	// 3. In case the SBE state is marked as CHECK_CFAM then SBE's
	// PERV_SB_MSG_FSI register must be checked to see if SBE is booted or
	// NOT.
	// check if SBE is in HALT state and perform HRESET if in HALT state
	// halt_state_check

	// get PIB target
	struct pdbg_target *pib = getPibTarget(proc);

	// Get the current SBE state
	enum sbe_state state;

	if (sbe_get_state(pib, &state)) {
		log(level::ERROR, "Failed to read SBE state information (%s)",
		    pdbg_target_path(proc));
		throw sbeError_t(exception::SBE_STATE_READ_FAIL);
	}

	// SBE_STATE_CHECK_CFAM case is already handled by pdbg api
	if (state == SBE_STATE_BOOTED)
		return;

	// TODO , check halt state and routine for recover SBE.

	if (state != SBE_STATE_BOOTED) {
		log(level::INFO,
		    "SBE (%s) is not ready for chip-op: state(0x%08x)",
		    pdbg_target_path(proc), state);
		throw sbeError_t(exception::SBE_CHIPOP_NOT_ALLOWED);
	}
}

sbeError_t captureFFDC(struct pdbg_target *proc)
{
	// get SBE FFDC info
	bufPtr_t bufPtr;
	uint32_t ffdcLen = 0;
	uint32_t status = 0;

	// get PIB target
	struct pdbg_target *pib = getPibTarget(proc);

	if (sbe_ffdc_get(pib, &status, bufPtr.getPtr(), &ffdcLen)) {
		log(level::ERROR, "sbe_ffdc_get function failed");
		throw sbeError_t(exception::SBE_FFDC_GET_FAILED);
	}
	// TODO Need to remove this once pdbg header file support in place
	const auto SBEFIFO_PRI_UNKNOWN_ERROR = 0x00FE0000;
	const auto SBEFIFO_SEC_HW_TIMEOUT = 0x0010;

	if (status == (SBEFIFO_PRI_UNKNOWN_ERROR | SBEFIFO_SEC_HW_TIMEOUT)) {
		log(level::INFO, "SBE chipop timeout reported(%s)",
		    pdbg_target_path(proc));
		return sbeError_t(exception::SBE_CMD_TIMEOUT);
	}

	// Handle empty buffer.
	if (!ffdcLen) {
		// log message and return.
		log(level::ERROR, "Empty SBE FFDC returned (%s)",
		    pdbg_target_path(proc));
		return sbeError_t(exception::SBE_FFDC_NO_DATA);
	}

	// create ffdc file
	tmpfile_t ffdcFile(bufPtr.getData(), ffdcLen);
	return sbeError_t(exception::SBE_CMD_FAILED, ffdcFile.getFd(),
			  ffdcFile.getPath().c_str());
}

void mpiplContinue(struct pdbg_target *proc)
{
	log(level::INFO, "Enter: mpiplContinue(%s)", pdbg_target_path(proc));

	// validate SBE state
	validateSBEState(proc);

	// get PIB target
	struct pdbg_target *pib = getPibTarget(proc);

	// call pdbg back-end function
	auto ret = sbe_mpipl_continue(pib);
	if (ret != 0) {
		throw captureFFDC(proc);
	}
}

void mpiplEnter(struct pdbg_target *proc)
{
	log(level::INFO, "Enter: mpiplEnter(%s)", pdbg_target_path(proc));

	// validate SBE state
	validateSBEState(proc);

	// get PIB target
	struct pdbg_target *pib = getPibTarget(proc);

	// call pdbg back-end function
	auto ret = sbe_mpipl_enter(pib);
	if (ret != 0) {
		throw captureFFDC(proc);
	}
}

void getTiInfo(struct pdbg_target *proc, uint8_t **data, uint32_t *dataLen)
{
	log(level::INFO, "Enter: getTiInfo(%s)", pdbg_target_path(proc));

	// validate SBE state
	validateSBEState(proc);

	// get PIB target
	struct pdbg_target *pib = getPibTarget(proc);

	// call pdbg back-end function
	auto ret = sbe_mpipl_get_ti_info(pib, data, dataLen);
	if (ret != 0) {
		throw captureFFDC(proc);
	}
}

} // namespace sbe
} // namespace openpower::phal