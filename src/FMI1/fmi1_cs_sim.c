/*
    Copyright (C) 2012 Modelon AB <http://www.modelon.com>

	You should have received a copy of the LICENCE-FMUChecker.txt
    along with this program. If not, contact Modelon AB.
*/
/**
	\file fmi1_cs_sim.c
	Simulation loop for the FMI 1.0 Co-simulation FMUs
*/
#include <fmuChecker.h>
#include <fmilib.h>

jm_status_enu_t fmi1_cs_simulate(fmu_check_data_t* cdata)
{	
	fmi1_status_t fmistatus;
	jm_status_enu_t jmstatus = jm_status_success;
	jm_callbacks* cb = &cdata->callbacks;

	fmi1_import_t* fmu = cdata->fmu1;
	fmi1_string_t fmuGUID = fmi1_import_get_GUID(fmu);
	fmi1_string_t mimeType;
	fmi1_real_t timeout = 0.0;
	fmi1_boolean_t visible = fmi1_false;
	fmi1_boolean_t interactive = fmi1_false;

	fmi1_real_t tstart = fmi1_import_get_default_experiment_start(fmu);
	fmi1_real_t tcur = tstart;
	fmi1_real_t hstep;
	fmi1_real_t tend = fmi1_import_get_default_experiment_stop(fmu);
	fmi1_boolean_t StopTimeDefined = fmi1_true;

	mimeType = fmi1_import_get_mime_type(fmu);
	if(	(cdata->fmu1_kind == fmi1_fmu_kind_enu_cs_standalone) 
		|| !mimeType || !mimeType[0])
	{
		mimeType = "application/x-fmu-sharedlibrary";
	}
	else {
		if(		(strcmp(mimeType, "application/x-fmu-sharedlibrary") != 0)
			&&  (strcmp(mimeType, "application/x-fmu-modelica") != 0)
			)
		{
			jm_log_info(cb, fmu_checker_module,"The FMU requests simulator with MIME type '%s'. Please, start it manually to perform the simulation.");
			return (jm_status_success);
		}
	}
	if(cdata->stopTime > 0) {
		tend = cdata->stopTime;
	}
	if(cdata->stepSize > 0) {
		hstep = cdata->stepSize;
	}
	else {
		hstep = (tend - tstart) / cdata->numSteps;
	}

	cdata->instanceName = "Test FMI 1.0 CS";
	jm_log_verbose(cb, fmu_checker_module, "Checker will instantiate the slave with \n"
		"\tFMU location ='%s'\n\tMIME type = '%s'", cdata->fmuLocation, mimeType);
	jmstatus = fmi1_import_instantiate_slave(fmu, cdata->instanceName, cdata->fmuLocation, mimeType, timeout, visible, interactive);
	if (jmstatus == jm_status_error) {
		jm_log_fatal(cb, fmu_checker_module, "Could not instantiate the model");
		return jm_status_error;
	}

	fmistatus = fmi1_import_initialize_slave(fmu, tstart, StopTimeDefined, tend);
	if((fmistatus == fmi1_status_ok) || (fmistatus == fmi1_status_warning)) {
		jm_log_info(cb, fmu_checker_module, "Initialized FMU for simulation starting at time %g", tstart);
		fmistatus = fmi1_status_ok;
	}
	else {
			jm_log_fatal(cb, fmu_checker_module, "Failed to initialize FMU for simulation (FMU status: %s)", fmi1_status_to_string(fmistatus));
			fmistatus = fmi1_status_fatal;
			jmstatus = jm_status_error;
	}

	while ((tcur <= tend) && (fmistatus == fmi1_status_ok)) {
		fmi1_boolean_t newStep = fmi1_true;
		if(tcur >= tend - 1e-3*hstep) /* last step should be on tend */
			tcur = tend;

		jm_log_verbose(cb, fmu_checker_module, "Simulation time: %g", tcur);

		fmistatus = fmi1_import_do_step(fmu, tcur, hstep, newStep);

		if(fmi1_write_csv_data(cdata, tcur) != jm_status_success){
			jmstatus = jm_status_error;
			break;
		}

		if((fmistatus == fmi1_status_ok) || (fmistatus == fmi1_status_warning)) {
			fmistatus = fmi1_status_ok;
		}
		else 
			break;
		tcur += hstep;
	}

	if((fmistatus != fmi1_status_ok) && (fmistatus != fmi1_status_warning)) {
		jm_log_fatal(cb, fmu_checker_module, "Simulation loop terminated since FMU returned status: %s", fmi1_status_to_string(fmistatus));
		jmstatus = jm_status_error;
	}
	else if(jmstatus != jm_status_error) {
 		 jm_log_info(cb, fmu_checker_module, "Simulation finished successfully");
	}

	fmistatus = fmi1_import_terminate_slave(fmu);

	if(  fmistatus != fmi1_status_ok) {
		 jm_log_error(cb, fmu_checker_module, "fmiTerminateSlave returned status: %s", fmi1_status_to_string(fmistatus));
	}

	fmi1_import_free_slave_instance(fmu);

	return jmstatus;
}