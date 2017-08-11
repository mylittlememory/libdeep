/*
  libdeep - a library for deep learning
  Copyright (C) 2015-2017  Bob Mottram <bob@freedombone.net>

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:
  1. Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.
  3. Neither the name of the University nor the names of its contributors
  may be used to endorse or promote products derived from this software
  without specific prior written permission.
  .
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE HOLDERS OR
  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "deeplearn_conv.h"

/**
 * @brief Create a number of convolutional layers
 * @param no_of_layers The number of layers
 * @param image_width Width of the input image or layer
 * @param image_height Height of the input image or layer
 * @param image_depth Depth of the input image
 * @param no_of_features The number of features to learn in the first layer
 * @param feature_width Width of features in the first layer
 * @param final_image_width Width of the final output layer
 * @param final_image_height Height of the final layer
 * @param match_threshold Array containing the minimum matching threshold
 *        for each convolution layer
 * @param conv Instance to be updated
 * @returns zero on success
 */
int conv_init(int no_of_layers,
              int image_width, int image_height, int image_depth,
              int no_of_features, int feature_width,
              int final_image_width, int final_image_height,
              float match_threshold[],
              deeplearn_conv * conv)
{
    conv->no_of_layers = no_of_layers;
    conv->current_layer = 0;
    conv->learning_rate = 0.1f;

    conv->itterations = 0;
    conv->training_ctr = 0;

    COUNTUP(l, no_of_layers) {
        conv->layer[l].width =
            image_width - ((image_width-final_image_width)*l/no_of_layers);

        if (l == 0)
            conv->layer[l].height =
                image_height - ((image_height-final_image_height)*l/no_of_layers);
        else
            conv->layer[l].height = conv->layer[l].width;

        if (l == 0)
            conv->layer[l].depth = image_depth;
        else
            conv->layer[l].depth = conv->layer[l-1].no_of_features;

        conv->layer[l].no_of_features = no_of_features;
        conv->layer[l].feature_width =
            feature_width*conv->layer[l].width/image_width;
        if (conv->layer[l].feature_width < 3)
            conv->layer[l].feature_width = 3;

        /* allocate memory for arrays */
        FLOATALLOC(conv->layer[l].layer,
                   conv->layer[l].width*conv->layer[l].height*
                   conv->layer[l].depth);
        if (!conv->layer[l].layer)
            return 1;

        FLOATALLOC(conv->layer[l].feature,
                   conv->layer[l].no_of_features*
                   conv->layer[l].feature_width*conv->layer[l].feature_width*
                   conv->layer[l].depth);
        if (!conv->layer[l].feature)
            return 2;
    }

    conv->outputs_width = final_image_width;
    conv->no_of_outputs =
        final_image_width*final_image_width*conv->layer[no_of_layers-1].depth;
    FLOATALLOC(conv->outputs, conv->no_of_outputs);
    if (!conv->outputs)
        return 3;

    FLOATALLOC(conv->match_threshold, conv->no_of_layers);
    if (!conv->match_threshold)
        return 4;
    memcpy((void*)conv->match_threshold, match_threshold,conv->no_of_layers*sizeof(float));

    return 0;
}

/**
 * @brief Frees memory for a preprocessing pipeline
 * @param conv Preprocessing object
 */
void conv_free(deeplearn_conv * conv)
{
    COUNTDOWN(l, conv->no_of_layers) {
        free(conv->layer[l].layer);
        free(conv->layer[l].feature);
    }

    free(conv->outputs);
    free(conv->match_threshold);
}

/**
 * @brief Uses gnuplot to plot the training error
 * @param conv Convolution object
 * @param filename Filename for the image to save as
 * @param title Title of the graph
 * @param img_width Width of the image in pixels
 * @param img_height Height of the image in pixels
 * @return zero on success
 */
int conv_plot_history(deeplearn_conv * conv,
                      char * filename, char * title,
                      int img_width, int img_height)
{
    int retval=0;
    FILE * fp;
    char data_filename[256];
    char plot_filename[256];
    char command_str[256];
    float value;
    float max_value = 0.01f;

    sprintf(data_filename,"%s%s",DEEPLEARN_TEMP_DIRECTORY,
            "libdeep_conv_data.dat");
    sprintf(plot_filename,"%s%s",DEEPLEARN_TEMP_DIRECTORY,
            "libdeep_conv_data.plot");

    /* save the data */
    fp = fopen(data_filename,"w");

    if (!fp)
        return -1;

    COUNTUP(index, conv->history_index) {
        value = conv->history[index];
        fprintf(fp,"%d    %.10f\n",
                index*conv->history_step,value);
        /* record the maximum error value */
        if (value > max_value)
            max_value = value;
    }
    fclose(fp);

    /* create a plot file */
    fp = fopen(plot_filename,"w");

    if (!fp)
        return -1;

    fprintf(fp,"%s","reset\n");
    fprintf(fp,"set title \"%s\"\n",title);
    fprintf(fp,"set xrange [0:%d]\n",
            conv->history_index*conv->history_step);
    fprintf(fp,"set yrange [0:%f]\n",max_value*102/100);
    fprintf(fp,"%s","set lmargin 9\n");
    fprintf(fp,"%s","set rmargin 2\n");
    fprintf(fp,"%s","set xlabel \"Time Step\"\n");
    fprintf(fp,"%s","set ylabel \"Training Error Percent\"\n");

    fprintf(fp,"%s","set grid\n");
    fprintf(fp,"%s","set key right top\n");

    fprintf(fp,"set terminal png size %d,%d\n",
            img_width, img_height);
    fprintf(fp,"set output \"%s\"\n", filename);
    fprintf(fp,"plot \"%s\" using 1:2 notitle with lines\n",
            data_filename);
    fclose(fp);

    /* run gnuplot using the created files */
    sprintf(command_str,"gnuplot %s", plot_filename);
    retval = system(command_str); /* I assume this is synchronous */

    /* remove temporary files */
    sprintf(command_str,"rm %s %s", data_filename,plot_filename);
    retval = system(command_str);

    return retval;
}

/**
 * @brief Saves the given convolution object to a file
 * @param fp File pointer
 * @param conv Convolution object
 * @return zero value on success
 */
int conv_save(FILE * fp, deeplearn_conv * conv)
{
    /*
    if (FLOATWRITE(conv->reduction_factor) == 0)
        return -1;


    COUNTUP(i, conv->no_of_layers) {
        if (INTWRITE(conv->layer[i].pooling_factor) == 0)
            return -18;
    }
    */

    return 0;
}

/**
 * @brief Loads a convolution object from file
 * @param fp File pointer
 * @param conv Convolution object
 * @return zero value on success
 */
int conv_load(FILE * fp, deeplearn_conv * conv)
{
    /*
    if (INTREAD(conv->reduction_factor) == 0)
        return -1;

    COUNTUP(i, conv->no_of_layers) {
        if (INTREAD(conv->layer[i].pooling_factor) == 0)
            return -20;
    }
    */

    return 0;
}

/**
 * @brief Convolves an input image or layer to an output layer
 * @param img Input image or previous layer
 * @param img_width Width of the image
 * @param img_height Height of the image
 * @param img_depth Depth of the image. If this is the first layer then it is
 *        the color depth, otherwise it is the number of features learned in
 *        the previous layer
 * @param feature_width Width if each image patch
 * @param no_of_features The number of features in the set
 * @param feature Array containing the learned features
 * @param layer The output layer
 * @param layer_width Width of the output layer. The total size of the
 *        output layer should be layer_width*layer_width*no_of_features
 */
void convolve_image(float img[],
                    int img_width, int img_height, int img_depth,
                    int feature_width, int no_of_features,
                    float feature[],
                    float layer[], int layer_width)
{
    float feature_pixels = 1.0f / (float)(feature_width*feature_width*img_depth);

    COUNTDOWN(layer_y, layer_width) {
        int ty = layer_y * img_height / layer_width;
        int by = (layer_y+1) * img_height / layer_width;
        COUNTDOWN(layer_x, layer_width) {
            int tx = layer_x * img_width / layer_width;
            int bx = (layer_x+1) * img_width / layer_width;
            COUNTDOWN(f, no_of_features) {
                float * curr_feature =
                    &feature[f*feature_width*feature_width*img_depth];

                float match = 0.0f;
                COUNTDOWN(yy, feature_width) {
                    int tyy = ty + (yy * (by-ty) / feature_width);
                    COUNTDOWN(xx, feature_width) {
                        int txx = tx + (xx * (bx-tx) / feature_width);
                        int n0 = ((tyy*img_width) + txx) * img_depth;
                        int n1 = ((yy * feature_width) + xx) * img_depth;
                        COUNTDOWN(d, img_depth) {
                            match +=
                                ((float)img[n0+d] - (float)curr_feature[n1+d])*
                                ((float)img[n0+d] - (float)curr_feature[n1+d]);
                        }
                    }
                }

                layer[((layer_y*layer_width) + layer_x)*no_of_features + f] =
                    1.0f - (match * feature_pixels);
            }
        }
    }
}

/**
 * @brief Feed forward to the given layer
 * @param img The input image
 * @param conv Convolution instance
 * @param layer The number of layers to convolve
 */
void conv_feed_forward(unsigned char * img,
                       deeplearn_conv * conv, int layer)
{
    /* convert the input image to floats */
    COUNTDOWN(i, conv->layer[0].width*conv->layer[0].height*conv->layer[0].depth)
        conv->layer[0].layer[i] = (float)img[i]/255.0f;

    COUNTUP(l, layer) {
        float * next_layer = conv->outputs;
        int next_layer_width = conv->outputs_width;

        if (l < conv->no_of_layers-1) {
            next_layer = conv->layer[l+1].layer;
            next_layer_width = conv->layer[l+1].width;
        }

        convolve_image(conv->layer[l].layer,
                       conv->layer[l].width, conv->layer[l].height,
                       conv->layer[l].depth,
                       conv->layer[l].feature_width,
                       conv->layer[l].no_of_features,
                       conv->layer[l].feature,
                       next_layer, next_layer_width);
    }
}

/**
 * @brief Learn features
 * @param conv Convolution instance
 * @param samples The number of samples from the image or layer
 * @param random_seed Random number generator seed
 * @returns matching score/error, with lower values being better match
 */
float conv_learn(unsigned char * img,
                 deeplearn_conv * conv,
                 int samples,
                 unsigned int * random_seed)
{
    float matching_score = 0;
    float * feature_score;
    int layer = conv->current_layer;

    if (layer >= conv->no_of_layers)
        return 0;

    FLOATALLOC(feature_score, conv->layer[layer].no_of_features);

    if (!feature_score)
        return -1;

    conv_feed_forward(img, conv, layer);

    COUNTDOWN(s, samples) {
        matching_score +=
            learn_features(conv->layer[layer].layer,
                           conv->layer[layer].width,
                           conv->layer[layer].height,
                           conv->layer[layer].depth,
                           conv->layer[layer].feature_width,
                           conv->layer[layer].no_of_features,
                           conv->layer[layer].feature,
                           feature_score,
                           samples, conv->learning_rate, random_seed);
        conv->itterations++;
    }

    free(feature_score);

    /* proceed to the next layer if the match is good enough */
    if (matching_score < conv->match_threshold[layer])
        conv->current_layer++;

    return matching_score;
}
