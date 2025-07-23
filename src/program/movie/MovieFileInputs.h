/*
    Copyright 2015-2024 Clément Gallet <clement.gallet@ens-lyon.org>

    This file is part of libTAS.

    libTAS is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    libTAS is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with libTAS.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIBTAS_MOVIEFILEINPUTS_H_INCLUDED
#define LIBTAS_MOVIEFILEINPUTS_H_INCLUDED

#include "ConcurrentQueue.h"
#include "shared/inputs/AllInputs.h"

#include <QtCore/QObject>
#include <fstream>
#include <string>
#include <vector>
#include <set>
#include <mutex>
#include <stdint.h>

struct Context;
class MovieFileChangeLog;
class IMovieAction;

class MovieFileInputs : public QObject {
    Q_OBJECT
public:

    /* Flag storing if the movie has been modified since last save.
     * Used for prompting a message when the game exits if the user wants
     * to save.
     */
    bool modifiedSinceLastSave;

    /* Flag storing if the movie has been modified since last autosave. */
    bool modifiedSinceLastAutoSave;

    /* Flag storing if the movie has been modified since last state loading.
     * Used to determine when a state loading increments the rerecord count. */
    bool modifiedSinceLastStateLoad;

    /* Queue of movie input changes that where pushed by the UI, to process by the main thread */
    ConcurrentQueue<IMovieAction*> action_queue;

    /* Movie length */
    int64_t length_sec, length_nsec;

    /* Variable framerate */
    bool variable_framerate;

    /* Prepare a movie file from the context */
    MovieFileInputs(Context* c);

    void setChangeLog(MovieFileChangeLog* mcl);

    void setFramerate(unsigned int num, unsigned int den, bool variable);

    /* Clear */
    void clear();

    /* Import the inputs into a list, and all the parameters.
     * Returns 0 if no error, or a negative value if an error occured */
    void load();

    /* Write the inputs into a file and compress to the whole moviefile */
    void save();

    /* Get the number of frames of the current movie */
    uint64_t nbFrames();

    /* Set inputs for a certain frame, and truncate if keep_inputs is false */
    int setInputs(const AllInputs& inputs, uint64_t pos, bool keep_inputs);

    /* Set inputs in the current frame, and truncate if keep_inputs is false */
    int setInputs(const AllInputs& inputs, bool keep_inputs);

    int setInputs(const AllInputs& inputs, uint64_t pos);
    int setInputs(const AllInputs& inputs);

    /* Load inputs from a certain frame */
    const AllInputs& getInputs(uint64_t pos);

    /* Load inputs from the current frame */
    const AllInputs& getInputs();

    /* Don't lock because it is locked already */
    const AllInputs& getInputsUnprotected(uint64_t pos);

    /* Clear a range of frame inputs */
    void clearInputs(int minFrame, int maxFrame);

    /* Paint a single input in a range of frames */
    void paintInput(SingleInput si, int value, int minFrame, int maxFrame);

    /* Paint a single input in a range of frames */
    void paintInput(SingleInput si, std::vector<int>& values, int minFrame);

    /* Edit a range of frame inputs */
    void editInputs(const std::vector<AllInputs>& inputs, uint64_t pos);
    void editInputs(const std::vector<AllInputs>& inputs, uint64_t pos, int count);

    /* Insert blank inputs before the requested pos */
    void insertInputsBefore(uint64_t pos, int count);

    /* Insert inputs from an array before the requested pos */
    void insertInputsBefore(const std::vector<AllInputs>& inputs, uint64_t pos);

    /* Delete inputs at the requested pos */
    void deleteInputs(uint64_t pos, int count);

    /* Extract all single inputs of all frames and insert them in the set */
    void extractInputs(std::set<SingleInput> &set);

    /* Copy inputs to another one */
    void copyFrom(const MovieFileInputs* movie_inputs);

    /* Close the moviefile */
    void close();

    /* Check if another movie has the same inputs as this movie, inside a
     * specified range of frames */
    bool isEqual(const MovieFileInputs* movie, unsigned int start_frame, unsigned int end_frame) const;

    /* Helper function called when the movie has been modified */
    void wasModified();

    /* Queue an input change in the movie, usually performed by the UI thread, 
     * so that it can applied by main thread */
    void queueInput(uint64_t pos, SingleInput si, int value, bool isEvent);
    
    /* Process all input events pushed by the UI thread */
    void processPendingActions();
    
    /* Return the movie frame count */
    uint64_t size();

    /* Compute the length of the movie file */
    void updateLength();
private:
    Context* context;

    MovieFileChangeLog* movie_changelog;

    /* Initial framerate values */
    unsigned int framerate_num, framerate_den;
    
    /* The list of inputs */
    std::vector<AllInputs> input_list;

    /* We need to protect the input list access, because both the main and UI
     * threads can read and write to the list */
    std::mutex input_list_mutex;
    
signals:
    void inputsToBeRemoved(int min_frame, int max_frame);
    void inputsRemoved(int min_frame, int max_frame);
    void inputsToBeInserted(int min_frame, int max_frame);
    void inputsInserted(int min_frame, int max_frame);
    void inputsToBeEdited(int min_frame, int max_frame);
    void inputsEdited(int min_frame, int max_frame);
    void inputsToBeReset();
    void inputsReset();

    friend class MovieActionEditFrames;
    friend class MovieActionInsertFrames;
    friend class MovieActionPaint;
    friend class MovieActionRemoveFrames;
};

#endif
