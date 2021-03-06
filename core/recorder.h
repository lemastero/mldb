/** recorder.h                                                     -*- C++ -*-
    Jeremy Barnes, 26 March 2016
    Copyright (c) 2016 mldb.ai inc.  All rights reserved.

    This file is part of MLDB. Copyright 2015 mldb.ai inc. All rights reserved.

    Interface for recording into MLDB.
*/

#include "mldb/sql/dataset_fwd.h"
#include "mldb/sql/dataset_types.h"
#include "mldb/types/value_description_fwd.h"
#include "mldb/core/mldb_entity.h"
#include "mldb/sql/cell_value.h"
#include "mldb/sql/expression_value.h"
#include <set>

// NOTE TO MLDB DEVELOPERS: This is an API header file.  No includes
// should be added, especially value_description.h.


#pragma once


namespace MLDB {


struct Recorder;

typedef EntityType<Recorder> RecorderType;


/*****************************************************************************/
/* RECORDER                                                                  */
/*****************************************************************************/

/** A recorder is used to record data into a dataset or another abstraction. */

struct Recorder: public MldbEntity {
    Recorder(MldbServer * server);

    MldbServer * server;

    virtual ~Recorder()
    {
    }

    virtual std::string getKind() const
    {
        return "recorder";
    }

    virtual Any getStatus() const;
    
    /** Record an expression value as a row.  This will be flattened by
        datasets that require flattening.
    */
    virtual void
    recordRowExpr(const RowPath & rowName,
                  const ExpressionValue & expr) = 0;

    /** See recordRowExpr().  This has the same effect, but takes an rvalue
        which is destroyed by the call.  This may result in performance
        improvements.
    */
    virtual void
    recordRowExprDestructive(RowPath rowName,
                             ExpressionValue expr);

    /** Record a pre-flattened row. */
    virtual void
    recordRow(const RowPath & rowName,
              const std::vector<std::tuple<ColumnPath, CellValue, Date> > & vals) = 0;

    /** See recordRow().  This has the same effect, but takes an rvalue
        which is destroyed by the call.  This may result in performance
        improvements.
    */
    virtual void
    recordRowDestructive(RowPath rowName,
                         std::vector<std::tuple<ColumnPath, CellValue, Date> > vals);

    /** Record multiple flattened rows in a single transaction.  Default
        implementation forwards to recordRow.
    */
    virtual void
    recordRows(const std::vector<std::pair<RowPath, std::vector<std::tuple<ColumnPath, CellValue, Date> > > > & rows) = 0;

    /** See recordRows().  This has the same effect, but takes an rvalue
        which is destroyed by the call.  This may result in performance
        improvements.
    */
    virtual void
    recordRowsDestructive(std::vector<std::pair<RowPath, std::vector<std::tuple<ColumnPath, CellValue, Date> > > > rows);

    /** Record multiple rows from ExpressionValues.  */
    virtual void
    recordRowsExpr(const std::vector<std::pair<RowPath, ExpressionValue > > & rows) = 0;

    /** See recordRowsExpr().  This has the same effect, but takes an rvalue
        which is destroyed by the call.  This may result in performance
        improvements.
    */
    virtual
    void recordRowsExprDestructive(std::vector<std::pair<RowPath, ExpressionValue > > rows);

    /** Return a function specialized to record the same set of atomic values
        over and over again into this chunk.

        The returned function can be called row by row, without needing to
        mention column names (inferred by the positions).  The values will
        be destroyed on insertion.  numVals must always equal the number of
        columns passed.

        Extra allows for unexpected columns to be recorded, for when the
        dataset contains some fixed columns but also may have dynamic
        columns.
    */
    virtual
    std::function<void (RowPath rowName,
                        Date timestamp,
                        CellValue * vals,
                        size_t numVals,
                        std::vector<std::pair<ColumnPath, CellValue> > extra)>
    specializeRecordTabular(const std::vector<ColumnPath> & columns);


    /** Default implementation of specializeRecordTabular.  It will construct
        a row and call recordRowDestructive.
    */
    void recordTabularImpl(RowPath rowName,
                           Date timestamp,
                           CellValue * vals,
                           size_t numVals,
                           std::vector<std::pair<ColumnPath, CellValue> > extra,
                           const std::vector<ColumnPath> & columnNames);
    

    virtual void finishedChunk();
};


/*****************************************************************************/
/* FUNCTIONS                                                                 */
/*****************************************************************************/

std::shared_ptr<Recorder>
createRecorder(MldbServer * server,
               const PolyConfig & config,
               const std::function<bool (const Json::Value & progress)> & onProgress);

DECLARE_STRUCTURE_DESCRIPTION_NAMED(RecorderPolyConfigDescription, PolyConfigT<Recorder>);

std::shared_ptr<RecorderType>
registerRecorderType(const Package & package,
                    const Utf8String & name,
                    const Utf8String & description,
                    std::function<Recorder * (RestDirectory *,
                                              PolyConfig,
                                              const std::function<bool (const Json::Value)> &)>
                        createEntity,
                    TypeCustomRouteHandler docRoute,
                    TypeCustomRouteHandler customRoute,
                    std::shared_ptr<const ValueDescription> config,
                    std::set<std::string> registryFlags);

/** Register a new recorder kind.  This takes care of registering everything behind
    the scenes.
*/
template<typename RecorderT, typename Config>
std::shared_ptr<RecorderType>
registerRecorderType(const Package & package,
                    const Utf8String & name,
                    const Utf8String & description,
                    const Utf8String & docRoute,
                    TypeCustomRouteHandler customRoute = nullptr,
                    std::set<std::string> registryFlags = {})
{
    return registerRecorderType
        (package, name, description,
         [] (RestDirectory * server,
             PolyConfig config,
             const std::function<bool (const Json::Value)> & onProgress)
         {
             std::shared_ptr<spdlog::logger> logger = MLDB::getMldbLog<RecorderT>();
             auto recorder = new RecorderT(RecorderT::getOwner(server), config, onProgress);
             recorder->logger = std::move(logger); // noexcept
             return recorder;
         },
         makeInternalDocRedirect(package, docRoute),
         customRoute,
         getDefaultDescriptionSharedT<Config>(),
         registryFlags);
}

template<typename RecorderT, typename Config>
struct RegisterRecorderType {
    RegisterRecorderType(const Package & package,
                         const Utf8String & name,
                         const Utf8String & description,
                         const Utf8String & docRoute,
                         TypeCustomRouteHandler customRoute = nullptr,
                         std::set<std::string> registryFlags = {})
    {
        handle = registerRecorderType<RecorderT, Config>
            (package, name, description, docRoute, customRoute, registryFlags);
    }

    std::shared_ptr<RecorderType> handle;
};

} // namespace MLDB

