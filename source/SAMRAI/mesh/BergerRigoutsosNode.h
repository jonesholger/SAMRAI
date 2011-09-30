/*************************************************************************
 *
 * This file is part of the SAMRAI distribution.  For full copyright
 * information, see COPYRIGHT and COPYING.LESSER.
 *
 * Copyright:     (c) 1997-2011 Lawrence Livermore National Security, LLC
 * Description:   Asynchronous Berger-Rigoutsos dendogram
 *
 ************************************************************************/
#ifndef included_mesh_BergerRigoutsosNode
#define included_mesh_BergerRigoutsosNode

#include "SAMRAI/SAMRAI_config.h"

#include "SAMRAI/tbox/AsyncCommGroup.h"
#include "SAMRAI/tbox/vector.h"
#include "SAMRAI/hier/BlockId.h"
#include "SAMRAI/hier/BoxContainerOrderedIterator.h"
#include "SAMRAI/hier/BoxLevel.h"
#include "SAMRAI/hier/Connector.h"
#include "SAMRAI/hier/PatchLevel.h"

#include <set>
#include <list>

namespace SAMRAI {
namespace mesh {

/*!
 * @brief Node in the asynchronous Berger-Rigoutsos (BR) dendogram.
 * Do not directly use this class; for clustering, use BergerRigoutsos
 * instead.
 *
 * In mesh generation, the BR algorithm can be used to cluster
 * tagged cells into boxes.
 * This algorithm is described in Berger and Rigoutsos,
 * IEEE Trans. on Sys, Man, and Cyber (21)5:1278-1286.
 *
 * This class implements the BR algorithm to execute
 * in a non-recursive way, in order to improve parallel
 * efficiency over recursive implementations.
 * To facilitate a non-recursive implementation,
 * data in the recursive tree is maintained in a "BR dendogram",
 * nodes of which are instances of this class.
 *
 * Clarification on the uses of the word "node":
 * - Dendogram node: Node in the BR dendogram (this class).
 * - Graph node: Node in a box graph.  The box graph is the form
 *   of the outputs of this class.  Each output graph node
 *   corresponds to a box generated by the BR algorithm.
 * - Processor: MPI process id.  This is called a node in some
 *   context.  For clarity, we avoid this use of "node".
 *
 * Each dendogram node is associated with a candidate box,
 * an owner process coordinating distributed computations on the box
 * and a group of processors participating in those computations.
 * Should the candidate box be one of the final output boxes,
 * the owner also owns the graph node associated with the box.
 *
 * To use this class:
 * -# Construct the root dendogram node, an object of type
 *    BergerRigoutsosNode.
 * -# Set the clustering parameters using setClusteringParameters().
 * -# Finetune the algorithm settings using the methods under
 *    "Algorithm settings".
 * -# Start clustering by calling clusterAndComputeRelationships().
 *
 * The 2 primary outputs of this implementation are:
 * -# A BoxLevel of Boxes containing input tags.  Each node
 *    corresponds to an output box.
 * -# Connector between the tag BoxLevel and the new BoxLevel.
 *
 * TODO:
 * -# Implement MOST_TAGS ownership option.  This may be an
 *    improvement over the MOST_OVERLAP and is easy to do
 *    because the number of local tags in the candidate box
 *    is already computed.
 */

class BergerRigoutsosNode:
   private tbox::AsyncCommStage::Handler
{

public:
   enum OwnerMode { SINGLE_OWNER = 0,
                    MOST_OVERLAP = 1,
                    FEWEST_OWNED = 2,
                    LEAST_ACTIVE = 3 };

   /*!
    * @brief Method for advancing the algorithm.
    *
    * Each corresponds to a choice permitted by setAlgorithmAdvanceMode().
    */
   enum AlgoAdvanceMode { ADVANCE_ANY,
                          ADVANCE_SOME,
                          SYNCHRONOUS };

   /*!
    * @brief Construct the root node of a BR dendogram.
    *
    * The root node is used to run the BR algorithm and
    * obtain outputs.
    */
   explicit BergerRigoutsosNode(
      const tbox::Dimension& dim,
      const hier::BlockId& block_id);

   /*!
    * @brief Destructor.
    *
    * Deallocate internal data.
    */
   ~BergerRigoutsosNode(
      void);

   /*!
    * @brief Set parameters for (our slight variation of) the
    * Berger-Rigoutsos algorithm.
    *
    * These parameters are not specific to the asynchronous algorithm
    * or DLBG.
    *
    * @param max_lap_cut_from_center: Limit the Laplace cut to this
    *   fraction of the distance from the center plane to the end.
    *   Zero means cut only at the center plane.  One means unlimited.
    *   Under most situations, one is fine.
    */
   void
   setClusteringParameters(
      const int tag_data_index,
      const int tag_val,
      const hier::IntVector min_box,
      const double efficiency_tol,
      const double combine_tol,
      const hier::IntVector& max_box_size,
      const double max_lap_cut_from_center);

   //@{
   //! @name Algorithm mode settings

   /*!
    * @brief Set the mode for advancing the asynchronous implementation.
    *
    * Choices are:
    * - @b "SYNCHRONOUS" --> wait for each communication stage to complete
    *   before moving on, thus resulting in synchronous execution.
    * - @b "ADVANCE_ANY" --> advance an dendogram node through its
    *   communication stage by using tbox::AsyncCommStage::advanceAny().
    * - @b "ADVANCE_SOME" --> advance an dendogram node through its
    *   communication stage by using tbox::AsyncCommStage::advanceSome().
    *
    * The default is "ADVANCE_SOME".
    *
    * Asynchronous modes are NOT guaranteed to compute the output
    * graph nodes in any particular order.  The order depends on
    * the ordering of message completion, which is not deterministic.
    * If you require consistent outputs, we suggest you have a scheme
    * for reordering the output boxes.
    */
   void
   setAlgorithmAdvanceMode(
      const std::string& algo_advance_mode);

   /*!
    * @brief Set the method for choosing the owner.
    * Choices:
    * - "MOST_OVERLAP"
    *   Ownership is given to the processor with the most
    *   overlap on the candidate box.  Default.
    * - "SINGLE_OWNER"
    *   In single-owner mode, the initial owner (process 0)
    *   always participates and owns all dendogram nodes.
    * - "FEWEST_OWNED"
    *   Choose the processor that owns the fewest dendogram
    *   nodes when the choice is made.  This is meant to
    *   relieve bottle-necks caused by excessive ownership.
    *   This option may lead to non-deterministic ownerships.
    * - "LEAST_ACTIVE"
    *   Choose the processor that participates in the fewest
    *   number of dendogram nodes when the choice is made.
    *   This is meant to relieve bottle-necks caused by
    *   excessive participation. This option may lead to
    *   non-deterministic ownerships.
    *
    * Experiments show that "MOST_OVERLAP" gives the best
    * clustering speed, while "SINGLE_OWNER" may give a faster
    * output globalization (since you don't need an all-gather).
    */
   void
   setOwnerMode(
      const std::string& mode);

   /*!
    * @brief Relationship computation flag.
    *
    * Valid mode values to set are:
    *
    * - "NONE" = No relationship computation.
    *
    * - "TAG_TO_NEW": Compute directed relationships from input (tagged) to
    * output (new) graph nodes.  With this option, it is possible to
    * determine output nodes neighboring any input nodes, but not
    * possible to determine input nodes neighboring a specific output
    * node.
    *
    * - "BIDIRECTIONAL": Compute directed relationships from input (tagged) to
    * output (new) graph nodes as well as the reverse.  With this
    * option, it is possible to determine output nodes neighboring any
    * input nodes, as well as input nodes neighboring any output node.
    * This is accomplished using an additional relationship-sharing
    * communication after all graph nodes have been created.
    *
    * The ghost_cell_width specifies the growth for the overlap
    * checks.  Overlap checking is done to determine nearest-neighbor
    * relationships when generating connectivity to new graph nodes.
    * If a box grown by this ammount intersects another box, the two
    * boxes are considered neighbors.
    *
    * By default, compute bidirectional relationships with a ghost cell width
    * of 1.
    */
   void
   setComputeRelationships(
      const std::string mode,
      const hier::IntVector& ghost_cell_width);

   //@}

   /*
    * @brief Run the clustering algorithm to generate the new BoxLevel
    * and compute relationships (if specified by setComputeRelationships()).
    *
    * If relationships computation is not specified, the Connectors are
    * unchanged.
    *
    * @param mpi_communicator Alternative MPI communicator.  If given,
    *   must be congruent with the tag mapped_box_level's MPI communicator.
    *   Specify tbox::SAMRAI_MPI::commNull if unused.  Highly recommend
    *   using an isolated communicator to prevent message mix-ups.
    */
   void
   clusterAndComputeRelationships(
      hier::BoxLevel& new_mapped_box_level,
      hier::Connector& tag_to_new,
      hier::Connector& new_to_tag,
      const hier::Box& bound_box,
      const tbox::Pointer<hier::PatchLevel> tag_level,
      const tbox::SAMRAI_MPI& mpi_object);

   //@{
   //! @name Access to outputs

   /*!
    * @brief Get the connectivity from the tagged nodes to the new nodes.
    *
    * The connectivity data generated depend on the flag set using
    * setComputeRelationships().
    */
   const hier::NeighborhoodSet&
   getNeighborhoodSetsToNew() const;

   /*!
    * @brief Get the connectivity from the new nodes back to the tagged nodes.
    *
    * The connectivity data generated depend on the flag set using
    * setComputeRelationships().
    */
   const hier::NeighborhoodSet&
   getNeighborhoodSetsFromNew() const;

   //@}

   //@{

   //! @name Developer's methods for analysis and debugging this class.
   virtual void
   printClassData(
      std::ostream& os,
      int detail_level = 0) const;

   //! @brief Global number of tags in clusters.
   int
   getNumTags() const;

   //! @brief Max number of tags owned.
   int
   getMaxTagsOwned() const;

   //! @brief Max number of local nodes for dendogram.
   int
   getMaxNodes() const;

   //! @brief max generation count for the local nodes in the dendogram.
   int
   getMaxGeneration() const;

   //! @brief Max number of locally owned nodes in the dendogram.
   int
   getMaxOwnership() const;

   //! @brief Average number of continuations for local nodes in dendogram.
   double
   getAvgNumberOfCont() const;

   //! @brief Max number of continuations for local nodes in dendogram.
   int
   getMaxNumberOfCont() const;

   /*!
    * @brief Number of boxes generated (but not necessarily owned)
    * on the local process.
    */
   int
   getNumBoxesGenerated() const;

   /*!
    * @brief Set whether to log dendogram node action history
    * (useful for debugging).
    */
   void
   setLogNodeHistory(
      bool flag);
   //@}

private:
   /*
    * Static integer constant defining value corresponding to a bad integer.
    */
   static const int BAD_INTEGER;

   /*!
    * @brief Shorthand for the box-graph node corresponding
    * to boxes.
    */
   typedef hier::Box Box;

   //! @brief Shorthand for a container of graph-nodes.
   typedef hier::BoxSet BoxSet;

   //! @brief Shorthand for a container of neighbor graph-nodes.
   typedef hier::Connector::NeighborSet GraphNeighborSet;

   //! @brief Shorthand for a sorted, possibly incontiguous, set of integers.
   typedef std::set<int> IntSet;

   /*!
    * @brief Type of vector<int> for internal use.
    *
    * Choose either std::vector or tbox::vector.  tbox::vector is a
    * wrapper for std::vector, adding array bounds checking.
    */
#ifdef DEBUG_CHECK_ASSERTIONS
   typedef tbox::vector<int> VectorOfInts;
#else
   typedef std::vector<int> VectorOfInts;
#endif

   /*!
    * @brief Parameters shared among all dendogram nodes in
    * a dendogram and collectively managed by those nodes.
    *
    * In the implementation of the BR algorithm, some parameters
    * are to be shared among all nodes in the dendogram,
    * either for efficiency or coordinating the dendogram nodes.
    * All such parameters are contained in a single CommonParams
    * object.
    */
   class CommonParams
   {
public:
      CommonParams(
         const tbox::Dimension& dim);

      const tbox::Dimension d_dim;

      /*!
       * @brief Queue on which to append jobs to be
       * launched or relaunched.
       *
       * What is placed on the queue depends on the version of
       * clusterAndComputeRelationships_...() used.
       *
       * We use void* to avoid having to explicitly instantiate
       * multiple types, although we only place BergerRigoutsosNode
       * pointers in the relaunch_queue.
       */
      std::list<BergerRigoutsosNode *> relaunch_queue;

      /*!
       * @brief Stage handling multiple asynchronous communication groups.
       */
      tbox::AsyncCommStage comm_stage;

      AlgoAdvanceMode algo_advance_mode;

      /*!
       * @brief Level where tags live.
       */
      tbox::Pointer<hier::PatchLevel> tag_level;

      /*!
       * @brief BoxLevel associated with tag_level.
       *
       * If relationships are computed (see setComputeRelationships()), the relationships
       * go between the graph nodes on the tagged level and the
       * generated graph nodes.
       */
      const hier::BoxLevel* tag_mapped_box_level;

      /*!
       * @brief New BoxSet generated by BR.
       *
       * This is where we store the boxes as we progress in the BR
       * algorithm.
       */
      hier::BoxSet new_mapped_box_set;

      /*!
       * @brief NeighborhoodSet from tag_mapped_box_level to new_mapped_box_level.
       *
       * This is where we store the relationships resulting from the BR algorithm.
       * The relationships are created locally for local nodes in tag_mapped_box_level.
       */
      hier::NeighborhoodSet tag_eto_new;

      /*!
       * @brief NeighborhoodSet from new_mapped_box_level to tag_mapped_box_level.
       *
       * The relationships are created when the owners of nodes in tag_mapped_box_level
       * share relationship data with owners of nodes in new_mapped_box_level.
       */
      hier::NeighborhoodSet new_eto_tag;

      /*!
       * @brief List of processes that will send neighbor data
       * for locally owned boxes after the BR algorithm completes.
       */
      IntSet relationship_senders;

      /*!
       * @brief Outgoing messages to be sent to graph node owners
       * describing new relationships found by local process.
       */
      std::map<int, VectorOfInts> relationship_messages;

      /*!
       * @brief If a candidate box does not fit in this limit,
       * it will be split.
       *
       * Boxes will not be recombined (@see combine_tol) if the
       * combination breaks this limit.
       *
       * This is meant to prevent huge boxes that degrades worst-case
       * performances in when later processing the box.
       */
      hier::IntVector max_box_size;

      //@{
      //@name Parameters from clustering algorithm interface
      int tag_data_index;
      int tag_val;
      hier::IntVector min_box;
      double efficiency_tol;
      double combine_tol;
      double max_lap_cut_from_center;
      //@}

      /*!
       * @brief Relationship computation flag.
       *
       * See setComputeRelationships().
       */
      int compute_relationships;

      //! @brief Ammount to grow a box when checking for overlap.
      hier::IntVector max_gcw;

      //! @brief How to chose the group's owner.
      OwnerMode owner_mode;

      //@{
      //! @name Communication parameters
      /*!
       * @brief MPI communicator used in all communications in
       * the dendogram.
       */
      tbox::SAMRAI_MPI mpi_object;
      int rank;
      int nproc;
      //! @brief Upperbound of valid tags.
      int tag_upper_bound;
      //! @brief Smallest unclaimed MPI tag in pool given to local process.
      int available_mpi_tag;
      //@}

      //@{
      //! @name Performance monitors
      static tbox::Pointer<tbox::Timer> t_cluster;
      static tbox::Pointer<tbox::Timer> t_cluster_and_compute_relationships;
      static tbox::Pointer<tbox::Timer> t_continue_algorithm;
      static tbox::Pointer<tbox::Timer> t_compute;
      static tbox::Pointer<tbox::Timer> t_comm_wait;
      static tbox::Pointer<tbox::Timer> t_MPI_wait;
      static tbox::Pointer<tbox::Timer> t_compute_new_graph_relationships;
      static tbox::Pointer<tbox::Timer> t_share_new_relationships;
      static tbox::Pointer<tbox::Timer> t_share_new_relationships_send;
      static tbox::Pointer<tbox::Timer> t_share_new_relationships_recv;
      static tbox::Pointer<tbox::Timer> t_share_new_relationships_unpack;
      static tbox::Pointer<tbox::Timer> t_local_tasks;
      static tbox::Pointer<tbox::Timer> t_local_histogram;
      /*
       * Multi-stage timers.  These are used in continueAlgorithm()
       * instead of the methods they time, because what they time may
       * include waiting for messages.  They are included in the
       * timer t_continue_algorithm.  They provide timing breakdown
       * for the different stages.
       */
      static tbox::Pointer<tbox::Timer> t_reduce_histogram;
      static tbox::Pointer<tbox::Timer> t_bcast_acceptability;
      static tbox::Pointer<tbox::Timer> t_gather_grouping_criteria;
      static tbox::Pointer<tbox::Timer> t_bcast_child_groups;
      static tbox::Pointer<tbox::Timer> t_bcast_to_dropouts;
      //@}

      //@{
      //! @name Auxiliary data for analysis and debugging.

      //! @brief Whether to log major actions of dendogram node.
      bool log_node_history;
      //! @brief Number of tags.
      int num_tags_in_all_nodes;
      //! @brief Max number of tags owned.
      int max_tags_owned;
      //! @brief Current number of dendogram nodes allocated.
      int num_nodes_allocated;
      //! @brief Highest number of dendogram nodes.
      int max_nodes_allocated;
      //! @brief Current number of dendogram nodes active.
      int num_nodes_active;
      //! @brief Highest number of dendogram nodes active.
      int max_nodes_active;
      //! @brief Current number of dendogram nodes owned.
      int num_nodes_owned;
      //! @brief Highest number of dendogram nodes owned.
      int max_nodes_owned;
      //! @brief Current number of dendogram nodes completed.
      int num_nodes_completed;
      //! @brief Highest number of generation.
      int max_generation;
      //! @brief Current number of boxes generated.
      int num_boxes_generated;
      //! @brief Number of continueAlgorithm calls for to complete nodes.
      int num_conts_to_complete;
      //! @brief Highest number of continueAlgorithm calls to complete nodes.
      int max_conts_to_complete;
      //@}
   };

   /*!
    * @brief Construct a non-root node.
    *
    * This is private because the object requires setting up
    * after constructing.  Nodes constructed this way are
    * only meant for internal use by the recursion mechanism.
    */
   BergerRigoutsosNode(
      CommonParams* common_params,
      mesh::BergerRigoutsosNode* parent,
      const int child_number,
      const hier::BlockId& block_id);

   /*
    * @brief Duplicate given MPI communicator for private use
    * and various dependent parameters.
    *
    * Requires that d_common->tag_mapped_box_level is already set!
    */
   void
   setMPI(
      const tbox::SAMRAI_MPI& mpi);

   /*!
    * @brief Run the BR algorithm to find boxes, then generate the relationships
    * between the tag mapped_box_level and the new mapped_box_level.
    */
   void
   clusterAndComputeRelationships();

   /*!
    * @brief Names of algorithmic phases while outside of
    * continueAlgorithm().
    *
    * "For_data_only" phase is when the dendogram node is only used to
    * store data. If the node is to be executed, it enters the
    * "to_be_launched" phase.
    *
    * All names beginning with "reduce", "gather" or "bcast"
    * refer to communication phases, where control is
    * returned before the algorithm completes.
    *
    * The "children" phase does not explicitly contain communication,
    * but the children may perform communication.
    *
    * The "completed" phase is when the algorithm has run to completion.
    * This is where the recursive implementation would return.
    *
    * The "deallocated" phase is for debugging.  This phase is
    * set by the destructor, just to help find dendogram nodes that
    * are deallocated but somehow was referenced.
    */
   enum WaitPhase { for_data_only,
                    to_be_launched,
                    reduce_histogram,
                    bcast_acceptability,
                    gather_grouping_criteria,
                    bcast_child_groups,
                    run_children,
                    bcast_to_dropouts,
                    completed,
                    deallocated };

   /*!
    * @brief MPI tags identifying messages.
    *
    * Each message tag is the d_mpi_tag plus a PhaseTag.
    * Originally, there were different tags for different
    * communication phases, determined by d_mpi_tag plus
    * a PhaseTag.  But this is not really needed,
    * so all phases use the tag d_mpi_tag.  The PhaseTag
    * type is just here in case we have to go back to using
    * them.
    */
   enum PhaseTag { reduce_histogram_tag = 0,
                   bcast_acceptability_tag = 0,
                   gather_grouping_criteria_tag = 0,
                   bcast_child_groups_tag = 0,
                   bcast_to_dropouts_tag = 0,
                   total_phase_tags = 1 };

   /*!
    * @brief Continue the the BR algorithm.
    *
    * Parameters for finding boxes are internal.
    * They should be set in the constructor.
    *
    * In parallel, this the method may return before
    * algorithm is completed.  In serial, no communication
    * is done, so the algorithm IS completed when this
    * method returns.  The method is completed if it
    * returns WaitPhase::completed.  This method may
    * and @em should be called multiple times as long as
    * the algorithm has not completed.
    *
    * If this method returns before the algorithm is
    * complete, this object will have put itself on
    * the leaf queue to be checked for completion later.
    *
    * @return The communication phase currently running.
    */
   WaitPhase
   continueAlgorithm();

   /*!
    * @brief Candidate box acceptance state.
    *
    * Note that accepted values are odd and rejected
    * and undetermined values are even!  See boxAccepted(),
    * boxRejected() and boxHasNoTag().
    *
    * It is not critical to have all values shown,
    * but the values help in debugging.
    *
    * Meaning of values:
    * - "hasnotag_by_owner": histogram is truly empty (after sum reduction).
    *   We don't accept the box, but we don't split it either.
    *   (This can only happen at the root dendogram node, as child
    *   boxes are guaranteed to have tags.)
    * - "(rejected|accepted)_by_calculation": decision by calculation
    *   on the owner process.
    * - "(rejected|accepted)_by_owner": decision by owner process,
    *   broadcast to participants.
    * - "(rejected|accepted)_by_recombination": decision by recombination
    *   on local process.
    * - "(rejected|accepted)_by_dropout_bcast": decision by participant group,
    *   broadcast
    *    to the dropout group.
    */
   enum BoxAcceptance { undetermined = -2,
                        hasnotag_by_owner = -1,
                        rejected_by_calculation = 0,
                        accepted_by_calculation = 1,
                        rejected_by_owner = 2,
                        accepted_by_owner = 3,
                        rejected_by_recombination = 4,
                        accepted_by_recombination = 5,
                        rejected_by_dropout_bcast = 6,
                        accepted_by_dropout_bcast = 7 };

   //@{
   //! @name Delegated tasks for various phases of running algorithm.
   void
   makeLocalTagHistogram();
   void
   reduceHistogram_start();
   bool
   reduceHistogram_check();
   void
   computeMinimalBoundingBoxForTags();
   void
   acceptOrSplitBox();
   void
   broadcastAcceptability_start();
   bool
   broadcastAcceptability_check();
   void
   countOverlapWithLocalPatches();
   void
   gatherGroupingCriteria_start();
   bool
   gatherGroupingCriteria_check();
   //! @brief Form child groups from gathered overlap counts.
   void
   formChildGroups();
   //! @brief Form child groups from local copy of all level boxes.
   void
   broadcastChildGroups_start();
   bool
   broadcastChildGroups_check();
   void
   runChildren_start();
   bool
   runChildren_check();
   void
   broadcastToDropouts_start();
   bool
   broadcastToDropouts_check();
   void
   createBox();
   void
   eraseBox();
   //! @brief Compute new graph relationships touching local tag nodes.
   void
   computeNewNeighborhoodSets();
   //! @brief Participants send new relationship data to graph node owners.
   void
   shareNewNeighborhoodSetsWithOwners();
   //@}

   //@{
   //! @name Utilities for implementing algorithm

   //! @brief Find the index of the owner in the group.
   int
   findOwnerInGroup(
      int owner,
      const VectorOfInts& group) const;
   //! @brief Claim a unique tag from process's available tag pool.
   void
   claimMPITag();
   /*!
    * @brief Heuristically determine "best" tree degree for
    * communication group size.
    */
   int
   computeCommunicationTreeDegree(
      int group_size) const;

   bool
   findZeroCutSwath(
      int& cut_lo,
      int& cut_hi,
      const int dim);

   void
   cutAtLaplacian(
      int& cut_pt,
      const int dim);

   int
   getHistogramBufferSize(
      const hier::Box& box) const;
   int *
   putHistogramToBuffer(
      int* buffer);
   int *
   getHistogramFromBuffer(
      int* buffer);
   int *
   putBoxToBuffer(
      const hier::Box& box,
      int* buffer) const;
   int *
   getBoxFromBuffer(
      hier::Box& box,
      int* buffer) const;
   //! @brief Compute list of non-participating processes.
   void
   computeDropoutGroup(
      const VectorOfInts& main_group,
      const VectorOfInts& sub_group,
      VectorOfInts& dropouts,
      const int add_group) const;
   BoxAcceptance
   intToBoxAcceptance(
      int i) const;
   bool boxAccepted() const {
      return bool(d_box_acceptance >= 0 &&
                  d_box_acceptance % 2);
   }
   bool boxRejected() const {
      return bool(d_box_acceptance >= 0 &&
                  d_box_acceptance % 2 == 0);
   }
   bool boxHasNoTag() const {
      return bool(d_box_acceptance == -1);
   }
   //@}

   //@{
   //! @name Utilities to help analysis and debugging
   // tbox::List<BergerRigoutsosNode*>::Iterator
   std::list<BergerRigoutsosNode *>::const_iterator
   inRelaunchQueue(
      BergerRigoutsosNode* node_ptr) const;
   bool
   inGroup(
      VectorOfInts& group,
      int rank = -1) const;
   void
   printState(
      std::ostream& co) const;
   void
   printDendogramState(
      std::ostream& co,
      const std::string& border) const;
   //@}

   /*!
    * @brief Initialize static objects and register shutdown routine.
    *
    * Only called by StartupShutdownManager.
    */
   static void
   initializeCallback();

   /*!
    * @brief Method registered with ShutdownRegister to cleanup statics.
    *
    * Only called by StartupShutdownManager.
    */
   static void
   finalizeCallback();

   const tbox::Dimension d_dim;

   /*!
    * @brief Unique id in the binary dendogram.
    *
    * - To have succinct formula, the root dendogram node has d_pos of 1.
    * - Parent id is d_pos/2
    * - Left child id is 2*d_pos
    * - Right child id is 2*d_pos+1
    * - Generation number is ln(d_pos)
    *
    * This parameter is only used for debugging.
    *
    * The id of a node grows exponentially with each generation.
    * If the position in the binary tree is too big to be represented
    * by an integer, d_pos is set to -1 for a left child and -2 for a
    * right child.
    */
   const int d_pos;

   /*!
    * @brief Common parameters shared with descendents and ancestors.
    *
    * Only the root of the tree allocates the common parameters.
    * For all others, this pointer is set by the parent.
    */
   CommonParams* d_common;

   //@{
   /*!
    * @name Tree-related data
    */

   //! @brief Parent node (or NULL for the root node).
   BergerRigoutsosNode* d_parent;

   //! @brief Left child.
   BergerRigoutsosNode* d_lft_child;

   //! @brief Right child.
   BergerRigoutsosNode* d_rht_child;

   //@}

   //@{
   /*!
    * @name Data for one recursion of the BR algorithm
    */

   /*
    * These parameters are listed roughly in order of usage.
    */

   hier::Box d_box;
   int d_owner;

   /*!
    * @name Id of participating processes.
    */
   VectorOfInts d_group;

   /*!
    * @brief MPI tag for message within a dendogram node.
    *
    * The tag is determined by on the process that owns the parent
    * when the parent decides to split its box.  The tags are broadcasted
    * along with the children boxes.
    */
   int d_mpi_tag;

   /*!
    * @brief Overlap count with d_box.
    */
   int d_overlap;

   /*!
    * @brief Whether and how box is accepted.
    *
    * @see BoxAcceptance.
    */
   BoxAcceptance d_box_acceptance;

   /*!
    * @brief Histogram for all dimensions of box d_box.
    *
    * If local process is d_owner, this is initially the
    * local histogram, then later, the reduced histogram.
    * If not, it is just the local histogram.
    */
   VectorOfInts d_histogram[tbox::Dimension::MAXIMUM_DIMENSION_VALUE];

   /*!
    * @brief Number of tags in the candidate box.
    */
   int d_num_tags;

   /*!
    * @brief Distributed graph node corresponding to an accepted box.
    *
    * On the owner process, this belongs in a hier::BoxLevel
    * object.  On contributor nodes, this is used to identify the
    * Box assigned by the owner.  The Box is important for
    * computing neighbor data.
    */
   hier::Box d_mapped_box;

   /*!
    * @brief Box iterator corresponding to an accepted box on
    * the owner.
    *
    * This is relevant only on the owner, where the d_mapped_box is
    * in a container.  On contributors, the graph node is non-local
    * and stands alone.
    */
   BoxSet::OrderedIterator d_mapped_box_iterator;

   /*!
    * @brief Name of wait phase when continueAlgorithm()
    * exits before completion.
    */
   WaitPhase d_wait_phase;

   //@}

   //@{
   /*!
    * @name Lower-level parameters for communication.
    */

   //! @brief Buffer for organizing outgoing data.
   VectorOfInts d_send_msg;
   //! @brief Buffer for organizing incoming data.
   VectorOfInts d_recv_msg;

   tbox::AsyncCommGroup* d_comm_group;
   //@}

   hier::BlockId d_block_id;

   //@{
   //! @name Deubgging aid

   /*!
    * @brief Generation number.
    *
    * The generation number is the parent's generation number plus 1.
    * The root has generation number 1.
    */
   const int d_generation;

   //! @brief Number of times continueAlgorithm was called.
   int d_n_cont;

   //@}

   /*
    * Static initialization and cleanup handler.
    */

   static tbox::StartupShutdownManager::Handler
      s_initialize_handler;
};

}
}

#endif  // included_mesh::BergerRigoutsosNode
